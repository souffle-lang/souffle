/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ast_utils_test.cpp
 *
 * Tests souffle's AST utils.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/Ground.h"
#include "ast/utility/Utils.h"
#include "parser/ParserDriver.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast {

namespace test {

TEST(AstUtils, Grounded) {
    // create an example clause:
    auto clause = mk<Clause>("r");

    // something like:
    //   r(X,Y,Z) :- a(X), X = Y, !b(Z).

    // r(X,Y,Z)
    auto* head = clause->getHead();
    head->addArgument(Own<Argument>(new Variable("X")));
    head->addArgument(Own<Argument>(new Variable("Y")));
    head->addArgument(Own<Argument>(new Variable("Z")));

    // a(X)
    auto* a = new Atom("a");
    a->addArgument(Own<Argument>(new Variable("X")));
    clause->addToBody(Own<Literal>(a));

    // X = Y
    Literal* e1 = new BinaryConstraint(
            BinaryConstraintOp::EQ, Own<Argument>(new Variable("X")), Own<Argument>(new Variable("Y")));
    clause->addToBody(Own<Literal>(e1));

    // !b(Z)
    auto* b = new Atom("b");
    b->addArgument(Own<Argument>(new Variable("Z")));
    auto* neg = new Negation(Own<Atom>(b));
    clause->addToBody(Own<Literal>(neg));

    // check construction
    EXPECT_EQ("r(X,Y,Z) :- \n   a(X),\n   X = Y,\n   !b(Z).", toString(*clause));

    auto program = mk<Program>();
    program->addClause(std::move(clause));
    DebugReport dbgReport;
    ErrorReport errReport;
    TranslationUnit tu{std::move(program), errReport, dbgReport};

    // obtain groundness
    auto isGrounded = analysis::getGroundedTerms(tu, *tu.getProgram().getClauses()[0]);

    auto args = head->getArguments();
    // check selected sub-terms
    EXPECT_TRUE(isGrounded[args[0]]);   // X
    EXPECT_TRUE(isGrounded[args[1]]);   // Y
    EXPECT_FALSE(isGrounded[args[2]]);  // Z
}

TEST(AstUtils, GroundedRecords) {
    ErrorReport e;
    DebugReport d;
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .type N <: symbol
                 .type R = [ a : N, B : N ]

                 .decl r ( r : R )
                 .decl s ( r : N )

                 s(x) :- r([x,y]).

            )",
            e, d);

    Program& program = tu->getProgram();

    auto* clause = program.getClauses("s")[0];

    // check construction
    EXPECT_EQ("s(x) :- \n   r([x,y]).", toString(*clause));

    // obtain groundness
    auto isGrounded = analysis::getGroundedTerms(*tu, *clause);

    const Atom* s = clause->getHead();
    const auto* r = dynamic_cast<const Atom*>(clause->getBodyLiterals()[0]);

    EXPECT_TRUE(s);
    EXPECT_TRUE(r);

    // check selected sub-terms
    EXPECT_TRUE(isGrounded[s->getArguments()[0]]);
    EXPECT_TRUE(isGrounded[r->getArguments()[0]]);
}

TEST(AstUtils, ReorderClauseAtoms) {
    ErrorReport e;
    DebugReport d;

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .decl a,b,c,d,e(x:number)
                a(x) :- b(x), c(x), 1 != 2, d(y), !e(z), c(z), e(x).
                .output a()
            )",
            e, d);

    Program& program = tu->getProgram();
    EXPECT_EQ(5, program.getRelations().size());

    EXPECT_NE(program.getRelation("a"), nullptr);
    auto&& clauses = program.getClauses("a");
    EXPECT_EQ(1, clauses.size());

    auto&& clause = clauses[0];
    EXPECT_EQ("a(x) :- \n   b(x),\n   c(x),\n   1 != 2,\n   d(y),\n   !e(z),\n   c(z),\n   e(x).",
            toString(*clause));

    // Check trivial permutation
    Own<Clause> reorderedClause0 =
            Own<Clause>(reorderAtoms(clause, std::vector<std::size_t>({0, 1, 2, 3, 4})));
    EXPECT_EQ("a(x) :- \n   b(x),\n   c(x),\n   1 != 2,\n   d(y),\n   !e(z),\n   c(z),\n   e(x).",
            toString(*reorderedClause0));

    // Check more complex permutation
    Own<Clause> reorderedClause1 =
            Own<Clause>(reorderAtoms(clause, std::vector<std::size_t>({2, 3, 4, 1, 0})));
    EXPECT_EQ("a(x) :- \n   d(y),\n   c(z),\n   1 != 2,\n   e(x),\n   !e(z),\n   c(x),\n   b(x).",
            toString(*reorderedClause1));
}

TEST(AstUtils, RemoveEquivalentClauses) {
    ErrorReport e;
    DebugReport d;

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .decl a()
                a(). a(). a(). a(). a(). a(). a(). a(). a(). a(). a(). a(). a(). a(). a().
            )",
            e, d);

    Program& program = tu->getProgram();
    EXPECT_EQ(1, program.getRelations().size());

    EXPECT_NE(program.getRelation("a"), nullptr);
    EXPECT_EQ(15, program.getClauses("a").size());

    program.removeRelation("a");
    EXPECT_EQ(0, program.getClauses("a").size());
}

}  // namespace test
}  // namespace souffle::ast
