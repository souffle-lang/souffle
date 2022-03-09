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
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "parser/ParserDriver.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::test {

inline Own<TranslationUnit> makeATU(std::string program) {
    ErrorReport e;
    DebugReport d;
    return ParserDriver::parseTranslationUnit(program, e, d);
}

inline Own<Clause> makeClause(std::string name, Own<Argument> headArgument) {
    auto clause = mk<Clause>(name);
    auto headAtom = clause->getHead();
    headAtom->addArgument(std::move(headArgument));
    return clause;
}

TEST(Program, Parse) {
    ErrorReport e;
    DebugReport d;
    // check the empty program
    Own<TranslationUnit> empty = ParserDriver::parseTranslationUnit("", e, d);

    EXPECT_TRUE(empty->getProgram().getTypes().empty());
    EXPECT_TRUE(empty->getProgram().getRelations().empty());

    // check something simple
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                   .type Node <: symbol
                   .decl e ( a : Node , b : Node )
                   .decl r ( from : Node , to : Node )

                   r(X,Y) :- e(X,Y).
                   r(X,Z) :- r(X,Y), r(Y,Z).
            )",
            e, d);

    auto& prog = tu->getProgram();
    std::cout << prog << "\n";

    EXPECT_EQ(1, prog.getTypes().size());
    EXPECT_EQ(2, prog.getRelations().size());

    EXPECT_TRUE(prog.getRelation("e"));
    EXPECT_TRUE(prog.getRelation("r"));
    EXPECT_FALSE(prog.getRelation("n"));
}

#define TESTASTCLONEANDEQUAL(SUBTYPE, DL)                                       \
    TEST(Ast, CloneAndEqual##SUBTYPE) {                                         \
        ErrorReport e;                                                          \
        DebugReport d;                                                          \
        Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(DL, e, d); \
        Program& program = tu->getProgram();                                    \
        EXPECT_EQ(program, program);                                            \
        Own<Program> cl(clone(program));                                        \
        EXPECT_NE(cl.get(), &program);                                          \
        EXPECT_EQ(*cl, program);                                                \
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

// should *not* remove anything because `removeClause` operates by identity, not equality
TEST(Program, RemoveClauseByEquality) {
    auto atom = mk<Atom>("B");
    atom->addArgument(mk<Variable>("x"));
    auto sum = mk<Aggregator>(AggregateOp::SUM, mk<Variable>("x"));
    VecOwn<Literal> body;
    body.push_back(std::move(atom));
    sum->setBodyLiterals(std::move(body));

    auto tu1 = makeATU(".decl A,B(x:number) \n A(sum x : B(x)).");
    auto clauses_pre = tu1->getProgram().getClauses();
    EXPECT_EQ(1, clauses_pre.size());
    auto* clause_1 = clauses_pre[0];

    auto clause = makeClause("A", std::move(sum));
    auto& clause_2 = *clause;
    tu1->getProgram().addClause(std::move(clause));
    tu1->getProgram().removeClause(clause_2);  // remove the second (and equal-value) clause

    auto&& clauses_post = tu1->getProgram().getClauses();
    EXPECT_EQ(1, clauses_post.size());
    EXPECT_EQ(clause_1, clauses_post[0]);
}

TEST(Program, RemoveClauseByIdentity) {
    auto tu1 = makeATU(".decl A,B(x:number) \n A(sum x : B(x)).");
    auto clauses = tu1->getProgram().getClauses("A");
    EXPECT_EQ(1, clauses.size());
    tu1->getProgram().removeClause(*clauses[0]);

    auto tu2 = makeATU(".decl A,B(x:number)");
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(Program, AppendRelation) {
    auto tu1 = makeATU(".decl A,B,C(x:number)");
    auto& prog1 = tu1->getProgram();
    auto rel = mk<Relation>();
    rel->setQualifiedName("D");
    rel->addAttribute(mk<Attribute>("x", "number"));
    prog1.addRelation(std::move(rel));
    auto tu2 = makeATU(".decl A,B,C,D(x:number)");
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(Program, RemoveRelation) {
    auto tu1 = makeATU(".decl A,B,C(x:number)");
    EXPECT_TRUE(tu1->getProgram().removeRelation("B"));
    auto tu2 = makeATU(".decl A,C(x:number)");
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

}  // namespace souffle::ast::test
