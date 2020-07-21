/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ast_program_test.cpp
 *
 * Tests souffle's AST program.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "AggregateOp.h"
#include "DebugReport.h"
#include "ErrorReport.h"
#include "ParserDriver.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstAttribute.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::test {

inline std::unique_ptr<AstTranslationUnit> makeATU(std::string program) {
    ErrorReport e;
    DebugReport d;
    return ParserDriver::parseTranslationUnit(program, e, d);
}

inline std::unique_ptr<AstClause> makeClause(std::string name, std::unique_ptr<AstArgument> headArgument) {
    auto headAtom = std::make_unique<AstAtom>(name);
    headAtom->addArgument(std::move(headArgument));
    auto clause = std::make_unique<AstClause>();
    clause->setHead(std::move(headAtom));
    return clause;
}

TEST(AstProgram, Parse) {
    ErrorReport e;
    DebugReport d;
    // check the empty program
    std::unique_ptr<AstTranslationUnit> empty = ParserDriver::parseTranslationUnit("", e, d);

    EXPECT_TRUE(empty->getProgram()->getTypes().empty());
    EXPECT_TRUE(empty->getProgram()->getRelations().empty());

    // check something simple
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                   .type Node <: symbol
                   .decl e ( a : Node , b : Node )
                   .decl r ( from : Node , to : Node )

                   r(X,Y) :- e(X,Y).
                   r(X,Z) :- r(X,Y), r(Y,Z).
            )",
            e, d);

    auto* prog = tu->getProgram();
    std::cout << *prog << "\n";

    EXPECT_EQ(1, prog->getTypes().size());
    EXPECT_EQ(2, prog->getRelations().size());

    EXPECT_TRUE(getRelation(*prog, "e"));
    EXPECT_TRUE(getRelation(*prog, "r"));
    EXPECT_FALSE(getRelation(*prog, "n"));
}

#define TESTASTCLONEANDEQUAL(SUBTYPE, DL)                                                      \
    TEST(Ast, CloneAndEqual##SUBTYPE) {                                                        \
        ErrorReport e;                                                                         \
        DebugReport d;                                                                         \
        std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(DL, e, d); \
        AstProgram& program = *tu->getProgram();                                               \
        EXPECT_EQ(program, program);                                                           \
        std::unique_ptr<AstProgram> clone(program.clone());                                    \
        EXPECT_NE(clone.get(), &program);                                                      \
        EXPECT_EQ(*clone, program);                                                            \
    }

TESTASTCLONEANDEQUAL(Program,
        R"(
                 .decl r(a:number,b:number,c:number,d:number)
                 .decl s(x:symbol)
                 .decl a(x:number)
                 s("xxx").
                 r(X,Y,Z,W) :- a(X), 10 = Y, Y = Z, 8 + W = 12 + 14.
                 r(z + 2, x, y, a) :- s(b), b = to_string(a), z = x, a = y, x = 2, a = 2 + x.
            )");

TESTASTCLONEANDEQUAL(Record,
        R"(
                 .type N <: symbol
                 .type R = [ a : N, B : N ]

                 .decl r ( r : R )
                 .decl s ( r : N )

                 s(x) :- r([x,y]).

            )");

TESTASTCLONEANDEQUAL(SimpleTypes,
        R"(
                 .type A <: symbol
                 .type B <: symbol
                 .type U = A | B

                 .decl a ( x : A )
                 .decl b ( x : B )
                 .decl u ( x : U )

                 a(X) :- u(X).
                 b(X) :- u(X).
                 u(X) :- u(X).

                 a(X) :- b(X).
                 a(X) :- b(Y).

            )");

TESTASTCLONEANDEQUAL(ComplexTypes,
        R"(
                 .type A <: symbol
                 .type B <: number
                 .type U = B

                 .decl a ( x : A )
                 .decl b ( x : B )
                 .decl u ( x : U )

                 a(X) :- X < 10.
                 b(X) :- X < 10.
                 u(X) :- X < 10.

            )");

TESTASTCLONEANDEQUAL(ChainedTypes,
        R"(
                .type D <: symbol
                .type C = D
                .type B = C
                .type A = B

                .decl R1(x:A,y:B)
                .decl R2(x:C,y:D)
                .decl R4(x:A) output

                R4(x) :- R2(x,x),R1(x,x).
            )");

TESTASTCLONEANDEQUAL(NestedAggregates,
        R"(
                .type D <: symbol
                .decl r(x:D)

                r(x) :- r(y), x=cat(cat(x,x),x).
            )");

TESTASTCLONEANDEQUAL(RelationCopies,
        R"(
                .type D = number
                .decl a(a:D,b:D)
                .decl b(a:D,b:D)
                .decl c(a:D,b:D)
                .decl d(a:D,b:D)

                a(1,2).
                b(x,y) :- a(x,y).
                c(x,y) :- b(x,y).

                d(x,y) :- b(x,y), c(y,x).

            )");

/** test removeClause, addRelation and removeRelation */
TEST(AstProgram, RemoveClause) {
    auto atom = std::make_unique<AstAtom>("B");
    atom->addArgument(std::make_unique<AstVariable>("x"));
    auto sum = std::make_unique<AstAggregator>(AggregateOp::SUM, std::make_unique<AstVariable>("x"));
    std::vector<std::unique_ptr<AstLiteral>> body;
    body.push_back(std::move(atom));
    sum->setBody(std::move(body));

    auto tu1 = makeATU(".decl A,B(x:number) \n A(sum x : B(x)).");
    auto clause = makeClause("A", std::move(sum));

    tu1->getProgram()->removeClause(clause.get());
    auto tu2 = makeATU(".decl A,B(x:number)");
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstProgram, AppendAstRelation) {
    auto tu1 = makeATU(".decl A,B,C(x:number)");
    auto* prog1 = tu1->getProgram();
    auto rel = std::make_unique<AstRelation>();
    rel->setQualifiedName("D");
    rel->addAttribute(std::make_unique<AstAttribute>("x", "number"));
    prog1->addRelation(std::move(rel));
    auto tu2 = makeATU(".decl A,B,C,D(x:number)");
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstProgram, RemoveAstRelation) {
    auto tu1 = makeATU(".decl A,B,C(x:number)");
    auto* prog1 = tu1->getProgram();
    prog1->removeRelation("B");
    auto tu2 = makeATU(".decl A,C(x:number)");
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

}  // end namespace souffle::test
