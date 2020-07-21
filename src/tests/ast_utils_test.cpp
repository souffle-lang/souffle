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

#include "BinaryConstraintOps.h"
#include "DebugReport.h"
#include "ErrorReport.h"
#include "ParserDriver.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/analysis/AstGroundAnalysis.h"
#include "ast/transform/AstTransforms.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {
class AstRelation;

namespace test {

TEST(AstUtils, Grounded) {
    // create an example clause:
    auto clause = std::make_unique<AstClause>();

    // something like:
    //   r(X,Y,Z) :- a(X), X = Y, !b(Z).

    // r(X,Y,Z)
    auto* head = new AstAtom("r");
    head->addArgument(std::unique_ptr<AstArgument>(new AstVariable("X")));
    head->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Y")));
    head->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Z")));
    clause->setHead(std::unique_ptr<AstAtom>(head));

    // a(X)
    auto* a = new AstAtom("a");
    a->addArgument(std::unique_ptr<AstArgument>(new AstVariable("X")));
    clause->addToBody(std::unique_ptr<AstLiteral>(a));

    // X = Y
    AstLiteral* e1 = new AstBinaryConstraint(BinaryConstraintOp::EQ,
            std::unique_ptr<AstArgument>(new AstVariable("X")),
            std::unique_ptr<AstArgument>(new AstVariable("Y")));
    clause->addToBody(std::unique_ptr<AstLiteral>(e1));

    // !b(Z)
    auto* b = new AstAtom("b");
    b->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Z")));
    auto* neg = new AstNegation(std::unique_ptr<AstAtom>(b));
    clause->addToBody(std::unique_ptr<AstLiteral>(neg));

    // check construction
    EXPECT_EQ("r(X,Y,Z) :- \n   a(X),\n   X = Y,\n   !b(Z).", toString(*clause));

    auto program = std::make_unique<AstProgram>();
    program->addClause(std::move(clause));
    DebugReport dbgReport;
    ErrorReport errReport;
    AstTranslationUnit tu{std::move(program), errReport, dbgReport};

    // obtain groundness
    auto isGrounded = getGroundedTerms(tu, *tu.getProgram()->getClauses()[0]);

    auto args = head->getArguments();
    // check selected sub-terms
    EXPECT_TRUE(isGrounded[args[0]]);   // X
    EXPECT_TRUE(isGrounded[args[1]]);   // Y
    EXPECT_FALSE(isGrounded[args[2]]);  // Z
}

TEST(AstUtils, GroundedRecords) {
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .type N <: symbol
                 .type R = [ a : N, B : N ]

                 .decl r ( r : R )
                 .decl s ( r : N )

                 s(x) :- r([x,y]).

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    AstClause* clause = getClauses(program, "s")[0];

    // check construction
    EXPECT_EQ("s(x) :- \n   r([x,y]).", toString(*clause));

    // obtain groundness
    auto isGrounded = getGroundedTerms(*tu, *clause);

    const AstAtom* s = clause->getHead();
    const auto* r = dynamic_cast<const AstAtom*>(clause->getBodyLiterals()[0]);

    EXPECT_TRUE(s);
    EXPECT_TRUE(r);

    // check selected sub-terms
    EXPECT_TRUE(isGrounded[s->getArguments()[0]]);
    EXPECT_TRUE(isGrounded[r->getArguments()[0]]);
}

TEST(AstUtils, GroundTermPropagation) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D <: symbol
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = getClauses(program, "p")[0];

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   r = [x,y],\n   s = r,\n   s = [w,v],\n   [w,v] = [a,b].",
            toString(*a));

    std::unique_ptr<AstClause> res = ResolveAliasesTransformer::resolveAliases(*a);
    std::unique_ptr<AstClause> cleaned = ResolveAliasesTransformer::removeTrivialEquality(*res);

    EXPECT_EQ(
            "p(x,y) :- \n   p(x,y),\n   [x,y] = [x,y],\n   [x,y] = [x,y],\n   [x,y] = [x,y],\n   [x,y] = "
            "[x,y].",
            toString(*res));
    EXPECT_EQ("p(x,y) :- \n   p(x,y).", toString(*cleaned));
}

TEST(AstUtils, GroundTermPropagation2) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
               .type D <: symbol
               .decl p(a:D,b:D)

               p(a,b) :- p(x,y), x = y, x = a, y = b.
           )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = getClauses(program, "p")[0];

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   x = y,\n   x = a,\n   y = b.", toString(*a));

    std::unique_ptr<AstClause> res = ResolveAliasesTransformer::resolveAliases(*a);
    std::unique_ptr<AstClause> cleaned = ResolveAliasesTransformer::removeTrivialEquality(*res);

    EXPECT_EQ("p(b,b) :- \n   p(b,b),\n   b = b,\n   b = b,\n   b = b.", toString(*res));
    EXPECT_EQ("p(b,b) :- \n   p(b,b).", toString(*cleaned));
}

TEST(AstUtils, ResolveGroundedAliases) {
    // load some test program
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D <: symbol
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   r = [x,y],\n   s = r,\n   s = [w,v],\n   [w,v] = [a,b].",
            toString(*getClauses(program, "p")[0]));

    std::make_unique<ResolveAliasesTransformer>()->apply(*tu);

    EXPECT_EQ("p(x,y) :- \n   p(x,y).", toString(*getClauses(program, "p")[0]));
}

TEST(AstUtils, ResolveAliasesWithTermsInAtoms) {
    // load some test program
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D <: symbol
                .decl p(a:D,b:D)

                p(x,c) :- p(x,b), p(b,c), c = b+1, x=c+2.
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*getClauses(program, "p")[0]));

    std::make_unique<ResolveAliasesTransformer>()->apply(*tu);

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*getClauses(program, "p")[0]));
}

TEST(AstUtils, RemoveRelationCopies) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
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

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ(4, program.getRelations().size());

    RemoveRelationCopiesTransformer::removeRelationCopies(*tu);

    EXPECT_EQ(2, program.getRelations().size());
}

TEST(AstUtils, RemoveRelationCopiesOutput) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D = number
                .decl a(a:D,b:D)
                .decl b(a:D,b:D)
                .decl c(a:D,b:D)
                .output c
                .decl d(a:D,b:D)

                a(1,2).
                b(x,y) :- a(x,y).
                c(x,y) :- b(x,y).

                d(x,y) :- b(x,y), c(y,x).

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ(4, program.getRelations().size());

    RemoveRelationCopiesTransformer::removeRelationCopies(*tu);

    EXPECT_EQ(3, program.getRelations().size());
}

TEST(AstUtils, ReorderClauseAtoms) {
    ErrorReport e;
    DebugReport d;

    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .decl a,b,c,d,e(x:number)
                a(x) :- b(x), c(x), 1 != 2, d(y), !e(z), c(z), e(x).
                .output a()
            )",
            e, d);

    AstProgram& program = *tu->getProgram();
    EXPECT_EQ(5, program.getRelations().size());

    AstRelation* a = getRelation(program, "a");
    EXPECT_NE(a, nullptr);
    const auto& clauses = getClauses(program, *a);
    EXPECT_EQ(1, clauses.size());

    AstClause* clause = clauses[0];
    EXPECT_EQ("a(x) :- \n   b(x),\n   c(x),\n   1 != 2,\n   d(y),\n   !e(z),\n   c(z),\n   e(x).",
            toString(*clause));

    // Check trivial permutation
    std::unique_ptr<AstClause> reorderedClause0 =
            std::unique_ptr<AstClause>(reorderAtoms(clause, std::vector<unsigned int>({0, 1, 2, 3, 4})));
    EXPECT_EQ("a(x) :- \n   b(x),\n   c(x),\n   1 != 2,\n   d(y),\n   !e(z),\n   c(z),\n   e(x).",
            toString(*reorderedClause0));

    // Check more complex permutation
    std::unique_ptr<AstClause> reorderedClause1 =
            std::unique_ptr<AstClause>(reorderAtoms(clause, std::vector<unsigned int>({2, 3, 4, 1, 0})));
    EXPECT_EQ("a(x) :- \n   d(y),\n   c(z),\n   1 != 2,\n   e(x),\n   !e(z),\n   c(x),\n   b(x).",
            toString(*reorderedClause1));
}

/**
 * Test the equivalence (or lack of equivalence) of clauses using the MinimiseProgramTransfomer.
 */
TEST(AstUtils, CheckClausalEquivalence) {
    ErrorReport e;
    DebugReport d;

    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .decl A(x:number, y:number)
                .decl B(x:number)
                .decl C(x:number)

                A(0,0).
                A(0,0).
                A(0,1).

                B(1).

                C(z) :- A(z,y), A(z,x), x != 3, x < y, !B(x), y > 3, B(y).
                C(r) :- A(r,y), A(r,x), x != 3, x < y, !B(y), y > 3, B(y), B(x), x < y.
                C(x) :- A(x,a), a != 3, !B(a), A(x,b), b > 3, B(c), a < b, c = b.
            )",
            e, d);

    const auto& program = *tu->getProgram();

    // Resolve aliases to remove trivial equalities
    std::make_unique<ResolveAliasesTransformer>()->apply(*tu);
    auto aClauses = getClauses(program, "A");
    auto bClauses = getClauses(program, "B");
    auto cClauses = getClauses(program, "C");

    EXPECT_EQ(3, aClauses.size());
    EXPECT_EQ("A(0,0).", toString(*aClauses[0]));
    EXPECT_EQ("A(0,0).", toString(*aClauses[1]));
    EXPECT_EQ("A(0,1).", toString(*aClauses[2]));

    EXPECT_EQ(1, bClauses.size());
    EXPECT_EQ("B(1).", toString(*bClauses[0]));

    EXPECT_EQ(3, cClauses.size());
    EXPECT_EQ("C(z) :- \n   A(z,y),\n   A(z,x),\n   x != 3,\n   x < y,\n   !B(x),\n   y > 3,\n   B(y).",
            toString(*cClauses[0]));
    EXPECT_EQ(
            "C(r) :- \n   A(r,y),\n   A(r,x),\n   x != 3,\n   x < y,\n   !B(y),\n   y > 3,\n   B(y),\n   "
            "B(x).",
            toString(*cClauses[1]));
    EXPECT_EQ("C(x) :- \n   A(x,a),\n   a != 3,\n   !B(a),\n   A(x,b),\n   b > 3,\n   B(b),\n   a < b.",
            toString(*cClauses[2]));

    // Check equivalence of these clauses
    // -- A
    EXPECT_TRUE(MinimiseProgramTransformer::areBijectivelyEquivalent(aClauses[0], aClauses[1]));
    EXPECT_TRUE(MinimiseProgramTransformer::areBijectivelyEquivalent(aClauses[1], aClauses[0]));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(aClauses[1], aClauses[2]));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(aClauses[0], aClauses[2]));

    // -- C
    EXPECT_TRUE(MinimiseProgramTransformer::areBijectivelyEquivalent(cClauses[0], cClauses[2]));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(cClauses[0], cClauses[1]));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(cClauses[2], cClauses[1]));

    // Make sure equivalent (and only equivalent) clauses are removed by the minimiser
    std::make_unique<MinimiseProgramTransformer>()->apply(*tu);
    auto aMinClauses = getClauses(program, "A");
    auto bMinClauses = getClauses(program, "B");
    auto cMinClauses = getClauses(program, "C");

    EXPECT_EQ(2, aMinClauses.size());
    EXPECT_EQ("A(0,0).", toString(*aMinClauses[0]));
    EXPECT_EQ("A(0,1).", toString(*aMinClauses[1]));

    EXPECT_EQ(1, bMinClauses.size());
    EXPECT_EQ("B(1).", toString(*bMinClauses[0]));

    EXPECT_EQ(2, cMinClauses.size());
    EXPECT_EQ("C(z) :- \n   A(z,y),\n   A(z,x),\n   x != 3,\n   x < y,\n   !B(x),\n   y > 3,\n   B(y).",
            toString(*cMinClauses[0]));
    EXPECT_EQ(
            "C(r) :- \n   A(r,y),\n   A(r,x),\n   x != 3,\n   x < y,\n   !B(y),\n   y > 3,\n   B(y),\n   "
            "B(x).",
            toString(*cMinClauses[1]));
}

/**
 * Test the removal of redundancies within clauses using the MinimiseProgramTransformer.
 *
 * In particular, the removal of:
 *      - intraclausal literals equivalent to another literal in the body
 *          e.g. a(x) :- b(x), b(x), c(x). --> a(x) :- b(x), c(x).
 *      - clauses that are only trivially satisfiable
 *          e.g. a(x) :- a(x), x != 0. is only true if a(x) is already true
 */
TEST(AstUtils, RemoveClauseRedundancies) {
    ErrorReport e;
    DebugReport d;

    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .decl a,b,c(X:number)
                a(0).
                b(1).
                c(X) :- b(X).

                a(X) :- b(X), c(X).
                a(X) :- a(X).
                a(X) :- a(X), X != 1.

                q(X) :- a(X).

                .decl q(X:number)
                .output q()
            )",
            e, d);

    const auto& program = *tu->getProgram();

    // Invoking the `RemoveRelationCopiesTransformer` to create some extra redundancy
    // In particular: The relation `c` will be replaced with `b` throughout, creating
    // the clause b(x) :- b(x).
    std::make_unique<RemoveRelationCopiesTransformer>()->apply(*tu);
    EXPECT_EQ(nullptr, getRelation(program, "c"));
    auto bIntermediateClauses = getClauses(program, "b");
    EXPECT_EQ(2, bIntermediateClauses.size());
    EXPECT_EQ("b(1).", toString(*bIntermediateClauses[0]));
    EXPECT_EQ("b(X) :- \n   b(X).", toString(*bIntermediateClauses[1]));

    // Attempt to minimise the program
    std::make_unique<MinimiseProgramTransformer>()->apply(*tu);
    EXPECT_EQ(3, program.getRelations().size());

    auto aClauses = getClauses(program, "a");
    EXPECT_EQ(2, aClauses.size());
    EXPECT_EQ("a(0).", toString(*aClauses[0]));
    EXPECT_EQ("a(X) :- \n   b(X).", toString(*aClauses[1]));

    auto bClauses = getClauses(program, "b");
    EXPECT_EQ(1, bClauses.size());
    EXPECT_EQ("b(1).", toString(*bClauses[0]));

    auto qClauses = getClauses(program, "q");
    EXPECT_EQ(1, qClauses.size());
    EXPECT_EQ("q(X) :- \n   a(X).", toString(*qClauses[0]));
}

}  // namespace test
}  // namespace souffle
