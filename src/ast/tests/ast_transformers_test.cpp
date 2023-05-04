/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ast_transformers_test.cpp
 *
 * Tests souffle's AST transformers.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "ast/Clause.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/ClauseNormalisation.h"
#include "ast/transform/MagicSet.h"
#include "ast/transform/MinimiseProgram.h"
#include "ast/transform/RemoveRedundantRelations.h"
#include "ast/transform/RemoveRelationCopies.h"
#include "ast/transform/ResolveAliases.h"
#include "ast/utility/Utils.h"
#include "parser/ParserDriver.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StringUtil.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast::transform::test {
using namespace analysis;

QualifiedName qn(std::string_view s) {
    return QualifiedName::fromString(s);
}

TEST(Transformers, GroundTermPropagation) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    // load some test program
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
                .type D <: symbol
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            errorReport, debugReport);

    Program& program = tu->getProgram();

    // check types in clauses
    auto* a = program.getClauses(qn("p"))[0];

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   r = [x,y],\n   s = r,\n   s = [w,v],\n   [w,v] = [a,b].",
            toString(*a));

    Own<Clause> res = ResolveAliasesTransformer::resolveAliases(*a);
    Own<Clause> cleaned = ResolveAliasesTransformer::removeTrivialEquality(*res);

    EXPECT_EQ(
            "p(x,y) :- \n   p(x,y),\n   [x,y] = [x,y],\n   [x,y] = [x,y],\n   [x,y] = [x,y],\n   [x,y] = "
            "[x,y].",
            toString(*res));
    EXPECT_EQ("p(x,y) :- \n   p(x,y).", toString(*cleaned));
}

TEST(Transformers, GroundTermPropagation2) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    // load some test program
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
               .type D <: symbol
               .decl p(a:D,b:D)

               p(a,b) :- p(x,y), x = y, x = a, y = b.
           )",
            errorReport, debugReport);

    Program& program = tu->getProgram();

    // check types in clauses
    auto* a = program.getClauses(qn("p"))[0];

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   x = y,\n   x = a,\n   y = b.", toString(*a));

    Own<Clause> res = ResolveAliasesTransformer::resolveAliases(*a);
    Own<Clause> cleaned = ResolveAliasesTransformer::removeTrivialEquality(*res);

    EXPECT_EQ("p(b,b) :- \n   p(b,b),\n   b = b,\n   b = b,\n   b = b.", toString(*res));
    EXPECT_EQ("p(b,b) :- \n   p(b,b).", toString(*cleaned));
}

TEST(Transformers, ResolveGroundedAliases) {
    // load some test program
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
                .type D <: symbol
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            errorReport, debugReport);

    Program& program = tu->getProgram();

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   r = [x,y],\n   s = r,\n   s = [w,v],\n   [w,v] = [a,b].",
            toString(*program.getClauses(qn("p"))[0]));

    mk<ResolveAliasesTransformer>()->apply(*tu);

    EXPECT_EQ("p(x,y) :- \n   p(x,y).", toString(*program.getClauses(qn("p"))[0]));
}

TEST(Transformers, ResolveAliasesWithTermsInAtoms) {
    // load some test program
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
                .type D <: symbol
                .decl p(a:D,b:D)

                p(x,c) :- p(x,b), p(b,c), c = b+1, x=c+2.
            )",
            errorReport, debugReport);

    Program& program = tu->getProgram();

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*program.getClauses(qn("p"))[0]));

    mk<ResolveAliasesTransformer>()->apply(*tu);

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*program.getClauses(qn("p"))[0]));
}

/**
 * Test that copies of relations are removed by RemoveRelationCopiesTransformer
 *
 * A(1, 2).
 * B(x, y) :- A(x, y).
 * C(x, y) :- B(x, y).
 * D(x, y) :- C(x, y).
 *
 * -> D(x, y) :- A(x, y).
 *
 *  B and C can be removed
 *
 */

TEST(Transformers, RemoveRelationCopies) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    // load some test program
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
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
            errorReport, debugReport);

    Program& program = tu->getProgram();

    EXPECT_EQ(4, program.getRelations().size());

    RemoveRelationCopiesTransformer::removeRelationCopies(*tu);

    EXPECT_EQ(2, program.getRelations().size());
}

/**
 * Test that copies of relations are removed by RemoveRelationCopiesTransformer
 *
 * A(1, 2).
 * B(x, y) :- A(x, y).
 * C(x, y) :- B(x, y).
 * D(x, y) :- C(x, y).
 * .output C
 *
 * -> C(x, y) :- A(x, y).
 * -> D(x, y) :- C(x, y).
 *
 *  B can be removed, but not C as C is output
 *
 */
TEST(Transformers, RemoveRelationCopiesOutput) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);
    // load some test program
    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
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
            errorReport, debugReport);

    Program& program = tu->getProgram();

    EXPECT_EQ(4, program.getRelations().size());

    RemoveRelationCopiesTransformer::removeRelationCopies(*tu);

    EXPECT_EQ(3, program.getRelations().size());
}

/**
 * Test the equivalence (or lack of equivalence) of clauses using the MinimiseProgramTransfomer.
 */
TEST(Transformers, CheckClausalEquivalence) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
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
            errorReport, debugReport);

    const auto& program = tu->getProgram();

    // Resolve aliases to remove trivial equalities
    mk<ResolveAliasesTransformer>()->apply(*tu);
    auto aClauses = program.getClauses(qn("A"));
    auto bClauses = program.getClauses(qn("B"));
    auto cClauses = program.getClauses(qn("C"));

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
    const auto normA0 = NormalisedClause(aClauses[0]);
    const auto normA1 = NormalisedClause(aClauses[1]);
    const auto normA2 = NormalisedClause(aClauses[2]);
    EXPECT_TRUE(MinimiseProgramTransformer::areBijectivelyEquivalent(normA0, normA1));
    EXPECT_TRUE(MinimiseProgramTransformer::areBijectivelyEquivalent(normA1, normA0));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(normA1, normA2));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(normA0, normA2));

    // -- C
    const auto normC0 = NormalisedClause(cClauses[0]);
    const auto normC1 = NormalisedClause(cClauses[1]);
    const auto normC2 = NormalisedClause(cClauses[2]);
    EXPECT_TRUE(MinimiseProgramTransformer::areBijectivelyEquivalent(normC0, normC2));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(normC0, normC1));
    EXPECT_FALSE(MinimiseProgramTransformer::areBijectivelyEquivalent(normC2, normC1));

    // Make sure equivalent (and only equivalent) clauses are removed by the minimiser
    mk<MinimiseProgramTransformer>()->apply(*tu);
    auto&& aMinClauses = program.getClauses(qn("A"));
    auto&& bMinClauses = program.getClauses(qn("B"));
    auto&& cMinClauses = program.getClauses(qn("C"));

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
 * Test the equivalence (or lack of equivalence) of aggregators using the MinimiseProgramTransfomer.
 */
TEST(Transformers, CheckAggregatorEquivalence) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
                .decl A,B,C,D(X:number) input
                // first and second are equivalent
                D(X) :-
                    B(X),
                    X < max Y : { C(Y), B(Y), Y < 2 },
                    A(Z),
                    Z = sum A : { C(A), B(A), A > count : { A(M), C(M) } }.

                D(V) :-
                    B(V),
                    A(W),
                    W = sum test1 : { C(test1), B(test1), test1 > count : { C(X), A(X) } },
                    V < max test2 : { C(test2), B(test2), test2 < 2 }.

                // third not equivalent
                D(V) :-
                    B(V),
                    A(W),
                    W = min test1 : { C(test1), B(test1), test1 > count : { C(X), A(X) } },
                    V < max test2 : { C(test2), B(test2), test2 < 2 }.

                .output D()
            )",
            errorReport, debugReport);

    const auto& program = tu->getProgram();
    mk<MinimiseProgramTransformer>()->apply(*tu);

    // A, B, C, D should still be the relations
    EXPECT_EQ(4, program.getRelations().size());
    EXPECT_NE(nullptr, program.getRelation(qn("A")));
    EXPECT_NE(nullptr, program.getRelation(qn("B")));
    EXPECT_NE(nullptr, program.getRelation(qn("C")));
    EXPECT_NE(nullptr, program.getRelation(qn("D")));

    // D should now only have the two clauses non-equivalent clauses
    auto&& dClauses = program.getClauses(qn("D"));
    EXPECT_EQ(2, dClauses.size());
    EXPECT_EQ(
            "D(X) :- \n   B(X),\n   X < max Y : { C(Y),B(Y),Y < 2 },\n   A(Z),\n   Z = sum A : { C(A),B(A),A "
            "> count : { A(M),C(M) } }.",
            toString(*dClauses[0]));
    EXPECT_EQ(
            "D(V) :- \n   B(V),\n   A(W),\n   W = min test1 : { C(test1),B(test1),test1 > count : { "
            "C(X),A(X) } },\n   V < max test2 : { C(test2),B(test2),test2 < 2 }.",
            toString(*dClauses[1]));
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
TEST(Transformers, RemoveClauseRedundancies) {
    Global glb;
    ErrorReport errorReport;
    DebugReport debugReport(glb);

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
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
            errorReport, debugReport);

    const auto& program = tu->getProgram();
    // Invoking the `RemoveRelationCopiesTransformer` should not create redundancy.
    // In particular: Replacing relation `c` with `b` should not create a clause
    // `b(x) :- b(x)` from `c(x) :- b(x)`.
    mk<RemoveRelationCopiesTransformer>()->apply(*tu);
    EXPECT_EQ(nullptr, program.getRelation(qn("c")));
    auto&& bIntermediateClauses = program.getClauses(qn("b"));
    EXPECT_EQ(1, bIntermediateClauses.size());
    EXPECT_EQ("b(1).", toString(*bIntermediateClauses[0]));

    // Attempt to minimise the program
    mk<MinimiseProgramTransformer>()->apply(*tu);
    EXPECT_EQ(3, program.getRelations().size());

    auto&& aClauses = program.getClauses(qn("a"));
    EXPECT_EQ(2, aClauses.size());
    EXPECT_EQ("a(0).", toString(*aClauses[0]));
    EXPECT_EQ("a(X) :- \n   b(X).", toString(*aClauses[1]));

    auto&& bClauses = program.getClauses(qn("b"));
    EXPECT_EQ(1, bClauses.size());
    EXPECT_EQ("b(1).", toString(*bClauses[0]));

    auto&& qClauses = program.getClauses(qn("q"));
    EXPECT_EQ(1, qClauses.size());
    EXPECT_EQ("q(X) :- \n   a(X).", toString(*qClauses[0]));
}

/**
 * Test the magic-set transformation on an example that covers all subtransformers, namely:
 *      (1) NormaliseDatabaseTransformer
 *      (2) LabelDatabaseTransformer
 *      (3) AdornDatabaseTransformer
 *      (4) MagicSetTransformer
 */
TEST(Transformers, MagicSetComprehensive) {
    Global glb;
    ErrorReport e;
    DebugReport d(glb);

    Own<TranslationUnit> tu = ParserDriver::parseTranslationUnit(glb,
            R"(
                // Stratum 0 - Base Relations
                .decl BaseOne(X:number) magic
                .decl BaseTwo(X:number) magic
                .input BaseOne, BaseTwo

                // Stratum 1 [depends on: 0]
                .decl A(X:number) magic
                .decl B(X:number) magic
                A(X) :- BaseOne(X).
                A(X) :- BaseOne(X), B(X).
                B(X) :- BaseTwo(X), A(X).

                // Stratum 2 [depends on: 0,1]
                .decl C(X:number) magic
                C(X) :- BaseTwo(X), A(X), B(X), X != 1.

                // Stratum 3 [depends on: 0,1]
                .decl R(X:number) magic
                R(X) :- BaseTwo(X), A(X), B(X), X != 0.

                // Stratum 4 [depends on: 0,1,2,3]
                .decl D(X:number) magic
                D(X) :- BaseOne(X), A(X), !C(X), !R(X).

                // Stratum 4 - Query [depends on: 0,1,4]
                .decl Query(X:number) magic
                .output Query
                Query(X) :- BaseOne(X), D(X), A(X).
            )",
            e, d);

    auto& program = tu->getProgram();

    // Test helpers
    auto mappifyRelations = [&](const Program& program) {
        std::map<std::string, std::multiset<std::string>> result;
        for (const auto* rel : program.getRelations()) {
            std::multiset<std::string> clauseStrings;
            auto relName = rel->getQualifiedName();
            for (auto&& clause : program.getClauses(*rel)) {
                clauseStrings.insert(toString(*clause));
            }
            result[toString(relName)] = clauseStrings;
        }
        return result;
    };
    auto checkRelMapEq = [&](const std::map<std::string, std::multiset<std::string>> left,
                                 const std::map<std::string, std::multiset<std::string>> right) {
        EXPECT_EQ(left.size(), right.size());
        for (const auto& [name, clauses] : left) {
            EXPECT_TRUE(contains(right, name));
            EXPECT_EQ(clauses, right.at(name));
        }
    };

    /* Stage 1: Database normalisation */
    // Constants should now be extracted from the inequality constraints
    mk<MagicSetTransformer::NormaliseDatabaseTransformer>()->apply(*tu);
    const auto relations1 = program.getRelations();
    EXPECT_EQ(8, program.getRelations().size());
    EXPECT_EQ(7, program.getClauses().size());

    auto expectedNormalisation = std::map<std::string, std::multiset<std::string>>({
            {"BaseOne", {}},
            {"BaseTwo", {}},
            {"A", {"A(X) :- \n   BaseOne(X).", "A(X) :- \n   BaseOne(X),\n   B(X)."}},
            {"B", {"B(X) :- \n   BaseTwo(X),\n   A(X)."}},
            {"C", {"C(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   @abdul0 = 1."}},
            {"R", {"R(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   @abdul0 = 0."}},
            {"D", {"D(X) :- \n   BaseOne(X),\n   A(X),\n   !C(X),\n   !R(X)."}},
            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D(X),\n   A(X)."}},
    });
    checkRelMapEq(expectedNormalisation, mappifyRelations(program));

    /* Stage 2: Database negative and positive labelling, keeping only the relevant ones */
    /* Stage 2.1: Negative labelling */
    mk<MagicSetTransformer::LabelDatabaseTransformer::NegativeLabellingTransformer>()->apply(*tu);
    EXPECT_EQ(14, program.getRelations().size());
    EXPECT_EQ(14, program.getClauses().size());

    auto expectedNegLabelling = std::map<std::string, std::multiset<std::string>>({
            // Original strata - neglabels should appear on all negated appearances of relations
            {"BaseOne", {}},
            {"BaseTwo", {}},
            {"A", {"A(X) :- \n   BaseOne(X).", "A(X) :- \n   BaseOne(X),\n   B(X)."}},
            {"B", {"B(X) :- \n   BaseTwo(X),\n   A(X)."}},
            {"C", {"C(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   @abdul0 = 1."}},
            {"R", {"R(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   @abdul0 = 0."}},
            {"D", {"D(X) :- \n   BaseOne(X),\n   A(X),\n   !@neglabel.C(X),\n   !@neglabel.R(X)."}},
            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D(X),\n   A(X)."}},

            // Neglaelled strata - relations should be copied and neglabelled one stratum at a time
            {"@neglabel.A", {"@neglabel.A(X) :- \n   BaseOne(X).",
                                    "@neglabel.A(X) :- \n   BaseOne(X),\n   @neglabel.B(X)."}},
            {"@neglabel.B", {"@neglabel.B(X) :- \n   BaseTwo(X),\n   @neglabel.A(X)."}},
            {"@neglabel.C", {"@neglabel.C(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   "
                             "@abdul0 = 1."}},
            {"@neglabel.R", {"@neglabel.R(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   "
                             "@abdul0 = 0."}},
            {"@neglabel.D", {"@neglabel.D(X) :- \n   BaseOne(X),\n   A(X),\n   !@neglabel.C(X),\n   "
                             "!@neglabel.R(X)."}},
            {"@neglabel.Query", {"@neglabel.Query(X) :- \n   BaseOne(X),\n   D(X),\n   A(X)."}},
    });
    checkRelMapEq(expectedNegLabelling, mappifyRelations(program));

    /* Stage 2.2: Positive labelling */
    mk<MagicSetTransformer::LabelDatabaseTransformer::PositiveLabellingTransformer>()->apply(*tu);
    EXPECT_EQ(33, program.getRelations().size());
    EXPECT_EQ(27, program.getClauses().size());

    auto expectedPosLabelling = std::map<std::string, std::multiset<std::string>>({
            // Original strata should remain the same
            {"BaseOne", {}},
            {"BaseTwo", {}},
            {"A", {"A(X) :- \n   BaseOne(X).", "A(X) :- \n   BaseOne(X),\n   B(X)."}},
            {"B", {"B(X) :- \n   BaseTwo(X),\n   A(X)."}},
            {"C", {"C(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   @abdul0 = 1."}},
            {"R", {"R(X) :- \n   BaseTwo(X),\n   A(X),\n   B(X),\n   X != @abdul0,\n   @abdul0 = 0."}},
            {"D", {"D(X) :- \n   BaseOne(X),\n   A(X),\n   !@neglabel.C(X),\n   !@neglabel.R(X)."}},
            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D(X),\n   A(X)."}},

            // Poslabelled + neglabelled strata - poslabel all positive derived literals
            {"@neglabel.A", {"@neglabel.A(X) :- \n   BaseOne(X).",
                                    "@neglabel.A(X) :- \n   BaseOne(X),\n   @neglabel.B(X)."}},
            {"@neglabel.B", {"@neglabel.B(X) :- \n   BaseTwo(X),\n   @neglabel.A(X)."}},
            {"@neglabel.C", {"@neglabel.C(X) :- \n   BaseTwo(X),\n   @poscopy_1.A(X),\n   @poscopy_1.B(X),\n "
                             "  X != @abdul0,\n   "
                             "@abdul0 = 1."}},
            {"@neglabel.R", {"@neglabel.R(X) :- \n   BaseTwo(X),\n   @poscopy_2.A(X),\n   @poscopy_2.B(X),\n "
                             "  X != @abdul0,\n   "
                             "@abdul0 = 0."}},
            {"@neglabel.D",
                    {"@neglabel.D(X) :- \n   BaseOne(X),\n   @poscopy_3.A(X),\n   !@neglabel.C(X),\n   "
                     "!@neglabel.R(X)."}},
            {"@neglabel.Query",
                    {"@neglabel.Query(X) :- \n   BaseOne(X),\n   @poscopy_1.D(X),\n   @poscopy_4.A(X)."}},

            // Copies of input stratum
            {"@poscopy_1.BaseOne", {}},
            {"@poscopy_1.BaseTwo", {}},
            {"@poscopy_2.BaseOne", {}},
            {"@poscopy_2.BaseTwo", {}},
            {"@poscopy_3.BaseOne", {}},
            {"@poscopy_3.BaseTwo", {}},
            {"@poscopy_4.BaseOne", {}},
            {"@poscopy_4.BaseTwo", {}},
            {"@poscopy_5.BaseOne", {}},
            {"@poscopy_5.BaseTwo", {}},

            // Copies of {A,B} stratum
            {"@poscopy_1.A", {"@poscopy_1.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_1.A(X) :- \n   BaseOne(X),\n   @poscopy_1.B(X)."}},
            {"@poscopy_1.B", {"@poscopy_1.B(X) :- \n   BaseTwo(X),\n   @poscopy_1.A(X)."}},
            {"@poscopy_2.A", {"@poscopy_2.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_2.A(X) :- \n   BaseOne(X),\n   @poscopy_2.B(X)."}},
            {"@poscopy_2.B", {"@poscopy_2.B(X) :- \n   BaseTwo(X),\n   @poscopy_2.A(X)."}},
            {"@poscopy_3.A", {"@poscopy_3.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_3.A(X) :- \n   BaseOne(X),\n   @poscopy_3.B(X)."}},
            {"@poscopy_3.B", {"@poscopy_3.B(X) :- \n   BaseTwo(X),\n   @poscopy_3.A(X)."}},
            {"@poscopy_4.A", {"@poscopy_4.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_4.A(X) :- \n   BaseOne(X),\n   @poscopy_4.B(X)."}},
            {"@poscopy_4.B", {"@poscopy_4.B(X) :- \n   BaseTwo(X),\n   @poscopy_4.A(X)."}},

            // Copies of {D} stratum
            {"@poscopy_1.D", {"@poscopy_1.D(X) :- \n   BaseOne(X),\n   @poscopy_4.A(X),\n   "
                              "!@neglabel.C(X),\n   !@neglabel.R(X)."}},
    });
    checkRelMapEq(expectedPosLabelling, mappifyRelations(program));

    /* Stage 2.3: Remove unnecessary labelled relations */
    mk<RemoveRedundantRelationsTransformer>()->apply(*tu);
    EXPECT_EQ(12, program.getRelations().size());
    EXPECT_EQ(13, program.getClauses().size());

    auto expectedFullLabelling = std::map<std::string, std::multiset<std::string>>({
            // Original strata
            {"BaseOne", {}},
            {"BaseTwo", {}},
            {"A", {"A(X) :- \n   BaseOne(X).", "A(X) :- \n   BaseOne(X),\n   B(X)."}},
            {"B", {"B(X) :- \n   BaseTwo(X),\n   A(X)."}},
            {"D", {"D(X) :- \n   BaseOne(X),\n   A(X),\n   !@neglabel.C(X),\n   !@neglabel.R(X)."}},
            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D(X),\n   A(X)."}},

            // Neglabelled strata
            {"@neglabel.C", {"@neglabel.C(X) :- \n   BaseTwo(X),\n   @poscopy_1.A(X),\n   @poscopy_1.B(X),\n "
                             "  X != @abdul0,\n   "
                             "@abdul0 = 1."}},
            {"@neglabel.R", {"@neglabel.R(X) :- \n   BaseTwo(X),\n   @poscopy_2.A(X),\n   @poscopy_2.B(X),\n "
                             "  X != @abdul0,\n   "
                             "@abdul0 = 0."}},

            // Poslabelled strata
            {"@poscopy_1.A", {"@poscopy_1.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_1.A(X) :- \n   BaseOne(X),\n   @poscopy_1.B(X)."}},
            {"@poscopy_1.B", {"@poscopy_1.B(X) :- \n   BaseTwo(X),\n   @poscopy_1.A(X)."}},
            {"@poscopy_2.A", {"@poscopy_2.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_2.A(X) :- \n   BaseOne(X),\n   @poscopy_2.B(X)."}},
            {"@poscopy_2.B", {"@poscopy_2.B(X) :- \n   BaseTwo(X),\n   @poscopy_2.A(X)."}},
    });
    checkRelMapEq(expectedFullLabelling, mappifyRelations(program));

    /* Stage 3: Database adornment */
    /* Stage 3.1: Adornment algorithm */
    mk<MagicSetTransformer::AdornDatabaseTransformer>()->apply(*tu);
    EXPECT_EQ(19, program.getRelations().size());
    EXPECT_EQ(23, program.getClauses().size());

    auto expectedAdornment = std::map<std::string, std::multiset<std::string>>({
            {"BaseOne", {}},
            {"BaseTwo", {}},
            {"A", {"A(X) :- \n   BaseOne(X).", "A(X) :- \n   BaseOne(X),\n   B(X)."}},
            {"B", {"B(X) :- \n   BaseTwo(X),\n   A(X)."}},
            {"D", {"D(X) :- \n   BaseOne(X),\n   A(X),\n   !@neglabel.C(X),\n   !@neglabel.R(X)."}},

            {"@neglabel.C", {"@neglabel.C(X) :- \n   BaseTwo(X),\n   @poscopy_1.A.{b}(X),\n   "
                             "@poscopy_1.B.{b}(X),\n   X != @abdul0,\n   @abdul0 = 1."}},
            {"@neglabel.R", {"@neglabel.R(X) :- \n   BaseTwo(X),\n   @poscopy_2.A.{b}(X),\n   "
                             "@poscopy_2.B.{b}(X),\n   X != @abdul0,\n   @abdul0 = 0."}},
            {"@poscopy_1.A", {"@poscopy_1.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_1.A(X) :- \n   BaseOne(X),\n   @poscopy_1.B(X)."}},
            {"@poscopy_1.B", {"@poscopy_1.B(X) :- \n   BaseTwo(X),\n   @poscopy_1.A(X)."}},
            {"@poscopy_2.A", {"@poscopy_2.A(X) :- \n   BaseOne(X).",
                                     "@poscopy_2.A(X) :- \n   BaseOne(X),\n   @poscopy_2.B(X)."}},
            {"@poscopy_2.B", {"@poscopy_2.B(X) :- \n   BaseTwo(X),\n   @poscopy_2.A(X)."}},

            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D.{b}(X),\n   A.{b}(X)."}},
            {"A.{b}", {"A.{b}(X) :- \n   BaseOne(X).", "A.{b}(X) :- \n   BaseOne(X),\n   B.{b}(X)."}},
            {"B.{b}", {"B.{b}(X) :- \n   BaseTwo(X),\n   A.{b}(X)."}},
            {"D.{b}",
                    {"D.{b}(X) :- \n   BaseOne(X),\n   A.{b}(X),\n   !@neglabel.C(X),\n   !@neglabel.R(X)."}},
            {"@poscopy_1.A.{b}", {"@poscopy_1.A.{b}(X) :- \n   BaseOne(X).",
                                         "@poscopy_1.A.{b}(X) :- \n   BaseOne(X),\n   @poscopy_1.B.{b}(X)."}},
            {"@poscopy_1.B.{b}", {"@poscopy_1.B.{b}(X) :- \n   BaseTwo(X),\n   @poscopy_1.A.{b}(X)."}},
            {"@poscopy_2.A.{b}", {"@poscopy_2.A.{b}(X) :- \n   BaseOne(X).",
                                         "@poscopy_2.A.{b}(X) :- \n   BaseOne(X),\n   @poscopy_2.B.{b}(X)."}},
            {"@poscopy_2.B.{b}", {"@poscopy_2.B.{b}(X) :- \n   BaseTwo(X),\n   @poscopy_2.A.{b}(X)."}},

    });
    checkRelMapEq(expectedAdornment, mappifyRelations(program));

    /* Stage 2.3: Remove no longer necessary relations */
    mk<RemoveRedundantRelationsTransformer>()->apply(*tu);
    EXPECT_EQ(12, program.getRelations().size());
    EXPECT_EQ(13, program.getClauses().size());

    auto expectedFinalAdornment = std::map<std::string, std::multiset<std::string>>({
            {"BaseOne", {}},
            {"BaseTwo", {}},
            {"@neglabel.C", {"@neglabel.C(X) :- \n   BaseTwo(X),\n   @poscopy_1.A.{b}(X),\n   "
                             "@poscopy_1.B.{b}(X),\n   X != @abdul0,\n   @abdul0 = 1."}},
            {"@neglabel.R", {"@neglabel.R(X) :- \n   BaseTwo(X),\n   @poscopy_2.A.{b}(X),\n   "
                             "@poscopy_2.B.{b}(X),\n   X != @abdul0,\n   @abdul0 = 0."}},
            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D.{b}(X),\n   A.{b}(X)."}},
            {"A.{b}", {"A.{b}(X) :- \n   BaseOne(X).", "A.{b}(X) :- \n   BaseOne(X),\n   B.{b}(X)."}},
            {"B.{b}", {"B.{b}(X) :- \n   BaseTwo(X),\n   A.{b}(X)."}},
            {"D.{b}",
                    {"D.{b}(X) :- \n   BaseOne(X),\n   A.{b}(X),\n   !@neglabel.C(X),\n   !@neglabel.R(X)."}},
            {"@poscopy_1.A.{b}", {"@poscopy_1.A.{b}(X) :- \n   BaseOne(X).",
                                         "@poscopy_1.A.{b}(X) :- \n   BaseOne(X),\n   @poscopy_1.B.{b}(X)."}},
            {"@poscopy_1.B.{b}", {"@poscopy_1.B.{b}(X) :- \n   BaseTwo(X),\n   @poscopy_1.A.{b}(X)."}},
            {"@poscopy_2.A.{b}", {"@poscopy_2.A.{b}(X) :- \n   BaseOne(X).",
                                         "@poscopy_2.A.{b}(X) :- \n   BaseOne(X),\n   @poscopy_2.B.{b}(X)."}},
            {"@poscopy_2.B.{b}", {"@poscopy_2.B.{b}(X) :- \n   BaseTwo(X),\n   @poscopy_2.A.{b}(X)."}},

    });
    checkRelMapEq(expectedFinalAdornment, mappifyRelations(program));

    /* Stage 4: MST core transformation */
    mk<MagicSetTransformer::MagicSetCoreTransformer>()->apply(*tu);
    EXPECT_EQ(19, program.getRelations().size());
    EXPECT_EQ(26, program.getClauses().size());

    auto finalProgram = std::map<std::string, std::multiset<std::string>>({
            {"BaseOne", {}},
            {"BaseTwo", {}},

            {"@neglabel.C", {"@neglabel.C(X) :- \n   BaseTwo(X),\n   @poscopy_1.A.{b}(X),\n   "
                             "@poscopy_1.B.{b}(X),\n   X != @abdul0,\n   @abdul0 = 1."}},
            {"@neglabel.R", {"@neglabel.R(X) :- \n   BaseTwo(X),\n   @poscopy_2.A.{b}(X),\n   "
                             "@poscopy_2.B.{b}(X),\n   X != @abdul0,\n   @abdul0 = 0."}},

            {"A.{b}", {"A.{b}(X) :- \n   @magic.A.{b}(X),\n   BaseOne(X),\n   B.{b}(X).",
                              "A.{b}(X) :- \n   @magic.A.{b}(X),\n   BaseOne(X)."}},
            {"B.{b}", {"B.{b}(X) :- \n   @magic.B.{b}(X),\n   BaseTwo(X),\n   A.{b}(X)."}},
            {"D.{b}", {"D.{b}(X) :- \n   @magic.D.{b}(X),\n   BaseOne(X),\n   A.{b}(X),\n   "
                       "!@neglabel.C(X),\n   !@neglabel.R(X)."}},

            {"@poscopy_1.A.{b}", {"@poscopy_1.A.{b}(X) :- \n   @magic.@poscopy_1.A.{b}(X),\n   BaseOne(X).",
                                         "@poscopy_1.A.{b}(X) :- \n   @magic.@poscopy_1.A.{b}(X),\n   "
                                         "BaseOne(X),\n   @poscopy_1.B.{b}(X)."}},
            {"@poscopy_1.B.{b}", {"@poscopy_1.B.{b}(X) :- \n   @magic.@poscopy_1.B.{b}(X),\n   BaseTwo(X),\n "
                                  "  @poscopy_1.A.{b}(X)."}},
            {"@poscopy_2.A.{b}", {"@poscopy_2.A.{b}(X) :- \n   @magic.@poscopy_2.A.{b}(X),\n   BaseOne(X).",
                                         "@poscopy_2.A.{b}(X) :- \n   @magic.@poscopy_2.A.{b}(X),\n   "
                                         "BaseOne(X),\n   @poscopy_2.B.{b}(X)."}},
            {"@poscopy_2.B.{b}", {"@poscopy_2.B.{b}(X) :- \n   @magic.@poscopy_2.B.{b}(X),\n   BaseTwo(X),\n "
                                  "  @poscopy_2.A.{b}(X)."}},

            {"Query", {"Query(X) :- \n   BaseOne(X),\n   D.{b}(X),\n   A.{b}(X)."}},

            // Magic rules
            {"@magic.A.{b}", {"@magic.A.{b}(X) :- \n   @magic.B.{b}(X),\n   BaseTwo(X).",
                                     "@magic.A.{b}(X) :- \n   BaseOne(X),\n   D.{b}(X).",
                                     "@magic.A.{b}(X) :- \n   @magic.D.{b}(X),\n   BaseOne(X)."}},
            {"@magic.B.{b}", {"@magic.B.{b}(X) :- \n   @magic.A.{b}(X),\n   BaseOne(X)."}},
            {"@magic.D.{b}", {"@magic.D.{b}(X) :- \n   BaseOne(X)."}},
            {"@magic.@poscopy_1.A.{b}", {"@magic.@poscopy_1.A.{b}(X) :- \n   BaseTwo(X),\n   @abdul0 = 1.",
                                                "@magic.@poscopy_1.A.{b}(X) :- \n   "
                                                "@magic.@poscopy_1.B.{b}(X),\n   BaseTwo(X)."}},
            {"@magic.@poscopy_2.A.{b}", {"@magic.@poscopy_2.A.{b}(X) :- \n   BaseTwo(X),\n   @abdul0 = 0.",
                                                "@magic.@poscopy_2.A.{b}(X) :- \n   "
                                                "@magic.@poscopy_2.B.{b}(X),\n   BaseTwo(X)."}},
            {"@magic.@poscopy_1.B.{b}",
                    {"@magic.@poscopy_1.B.{b}(X) :- \n   BaseTwo(X),\n   @poscopy_1.A.{b}(X),\n   @abdul0 = "
                     "1.",
                            "@magic.@poscopy_1.B.{b}(X) :- \n   @magic.@poscopy_1.A.{b}(X),\n   "
                            "BaseOne(X)."}},
            {"@magic.@poscopy_2.B.{b}",
                    {"@magic.@poscopy_2.B.{b}(X) :- \n   BaseTwo(X),\n   @poscopy_2.A.{b}(X),\n   @abdul0 = "
                     "0.",
                            "@magic.@poscopy_2.B.{b}(X) :- \n   @magic.@poscopy_2.A.{b}(X),\n   "
                            "BaseOne(X)."}},
    });
    checkRelMapEq(finalProgram, mappifyRelations(program));
}
}  // namespace souffle::ast::transform::test
