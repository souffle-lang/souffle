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

#include "AstGroundAnalysis.h"
#include "AstTransforms.h"
#include "AstTranslationUnit.h"
#include "AstTypeAnalysis.h"
#include "AstTypeEnvironmentAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "ParserDriver.h"
#include "test.h"

namespace souffle {

namespace test {

TEST(AstUtils, Grounded) {
    // create an example clause:
    auto* clause = new AstClause();

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
    AstLiteral* e1 = new AstBinaryConstraint("=", std::unique_ptr<AstArgument>(new AstVariable("X")),
            std::unique_ptr<AstArgument>(new AstVariable("Y")));
    clause->addToBody(std::unique_ptr<AstLiteral>(e1));

    // !b(Z)
    auto* b = new AstAtom("b");
    b->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Z")));
    auto* neg = new AstNegation(std::unique_ptr<AstAtom>(b));
    clause->addToBody(std::unique_ptr<AstLiteral>(neg));

    // check construction
    EXPECT_EQ("r(X,Y,Z) :- \n   a(X),\n   X = Y,\n   !b(Z).", toString(*clause));

    // obtain groundness
    auto isGrounded = getGroundedTerms(*clause);

    auto args = head->getArguments();
    // check selected sub-terms
    EXPECT_TRUE(isGrounded[args[0]]);   // X
    EXPECT_TRUE(isGrounded[args[1]]);   // Y
    EXPECT_FALSE(isGrounded[args[2]]);  // Z

    // done
    delete clause;
}

TEST(AstUtils, GroundedRecords) {
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .type N
                 .type R = [ a : N, B : N ]


                 .decl r ( r : R )
                 .decl s ( r : N )

                 s(x) :- r([x,y]).

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    AstClause* clause = program.getRelation("s")->getClauses()[0];

    // check construction
    EXPECT_EQ("s(x) :- \n   r([x,y]).", toString(*clause));

    // obtain groundness
    auto isGrounded = getGroundedTerms(*clause);

    const AstAtom* s = clause->getHead();
    const auto* r = dynamic_cast<const AstAtom*>(clause->getBodyLiterals()[0]);

    EXPECT_TRUE(s);
    EXPECT_TRUE(r);

    // check selected sub-terms
    EXPECT_TRUE(isGrounded[s->getArguments()[0]]);
    EXPECT_TRUE(isGrounded[r->getArguments()[0]]);
}

TEST(AstUtils, SimpleTypes) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .type A
                 .type B
                 .type U = A | B

                 .decl a ( x : A )
                 .decl b ( x : B )
                 .decl u ( x : U )

                 a(X) :- u(X).
                 b(X) :- u(X).
                 u(X) :- u(X).

                 a(X) :- b(X).
                 a(X) :- b(Y).

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("a")->getClauses()[0];
    AstClause* b = program.getRelation("b")->getClauses()[0];
    AstClause* u = program.getRelation("u")->getClauses()[0];

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    auto getX = [](const AstClause* c) { return c->getHead()->getArguments()[0]; };

    EXPECT_EQ("{A}", toString(typeAnalysis->getTypes(getX(a))));
    EXPECT_EQ("{B}", toString(typeAnalysis->getTypes(getX(b))));
    EXPECT_EQ("{U}", toString(typeAnalysis->getTypes(getX(u))));

    AstClause* a1 = program.getRelation("a")->getClauses()[1];
    EXPECT_EQ("{}", toString(typeAnalysis->getTypes(getX(a1))));

    AstClause* a2 = program.getRelation("a")->getClauses()[2];
    EXPECT_EQ("{A}", toString(typeAnalysis->getTypes(getX(a2))));
}

TEST(AstUtils, NumericTypes) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .symbol_type A
                 .number_type B
                 .type U = B

                 .decl a ( x : A )
                 .decl b ( x : B )
                 .decl u ( x : U )

                 a(X) :- X < 10.
                 b(X) :- X < 10.
                 u(X) :- X < 10.

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("a")->getClauses()[0];
    AstClause* b = program.getRelation("b")->getClauses()[0];
    AstClause* u = program.getRelation("u")->getClauses()[0];

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    auto getX = [](const AstClause* c) { return c->getHead()->getArguments()[0]; };

    EXPECT_EQ("{}", toString(typeAnalysis->getTypes(getX(a))));
    EXPECT_EQ("{B}", toString(typeAnalysis->getTypes(getX(b))));
    EXPECT_EQ("{U}", toString(typeAnalysis->getTypes(getX(u))));
}

TEST(AstUtils, SubtypeChain) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .type C = D
                .type B = C
                .type A = B

                .decl R1(x:A,y:B)
                .decl R2(x:C,y:D)
                .decl R4(x:A) output

                R4(x) :- R2(x,x),R1(x,x).
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("R4")->getClauses()[0];

    auto getX = [](const AstClause* c) { return c->getHead()->getArguments()[0]; };

    // check proper type handling
    auto& env = tu->getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();
    EXPECT_PRED2(isSubtypeOf, env.getType("B"), env.getType("A"));
    EXPECT_PRED2(isSubtypeOf, env.getType("C"), env.getType("A"));
    EXPECT_PRED2(isSubtypeOf, env.getType("D"), env.getType("A"));

    EXPECT_PRED2(isSubtypeOf, env.getType("C"), env.getType("B"));
    EXPECT_PRED2(isSubtypeOf, env.getType("D"), env.getType("B"));

    EXPECT_PRED2(isSubtypeOf, env.getType("D"), env.getType("C"));

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    // check proper type deduction
    EXPECT_EQ("{D}", toString(typeAnalysis->getTypes(getX(a))));
}

TEST(AstUtils, FactTypes) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .symbol_type A
                 .number_type B

                 .type C
                 .type U = A | C

                 .decl a ( x : A )
                 .decl b ( x : B )
                 .decl u ( x : U )

                 a("Hello").
                 b(10).
                 u("World").

            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("a")->getClauses()[0];
    AstClause* b = program.getRelation("b")->getClauses()[0];
    AstClause* u = program.getRelation("u")->getClauses()[0];

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    auto getX = [](const AstClause* c) { return c->getHead()->getArguments()[0]; };

    EXPECT_EQ("{A}", toString(typeAnalysis->getTypes(getX(a))));
    EXPECT_EQ("{B}", toString(typeAnalysis->getTypes(getX(b))));
    EXPECT_EQ("{U}", toString(typeAnalysis->getTypes(getX(u))));
}

TEST(AstUtils, NestedFunctions) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl r(x:D)

                r(x) :- r(y), x=cat(cat(x,x),x).
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("r")->getClauses()[0];

    auto getX = [](const AstClause* c) { return c->getHead()->getArguments()[0]; };

    // check proper type deduction
    EXPECT_EQ("{D}", toString(tu->getAnalysis<TypeAnalysis>()->getTypes(getX(a))));
}

TEST(AstUtils, GroundTermPropagation) {
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("p")->getClauses()[0];

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
               .type D
               .decl p(a:D,b:D)

               p(a,b) :- p(x,y), x = y, x = a, y = b.
           )",
            e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("p")->getClauses()[0];

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
                .type D
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   r = [x,y],\n   s = r,\n   s = [w,v],\n   [w,v] = [a,b].",
            toString(*program.getRelation("p")->getClauses()[0]));

    std::make_unique<ResolveAliasesTransformer>()->apply(*tu);

    EXPECT_EQ("p(x,y) :- \n   p(x,y).", toString(*program.getRelation("p")->getClauses()[0]));
}

TEST(AstUtils, ResolveAliasesWithTermsInAtoms) {
    // load some test program
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl p(a:D,b:D)

                p(x,c) :- p(x,b), p(b,c), c = b+1, x=c+2.
            )",
            e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*program.getRelation("p")->getClauses()[0]));

    std::make_unique<ResolveAliasesTransformer>()->apply(*tu);

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*program.getRelation("p")->getClauses()[0]));
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

    AstRelation* a = program.getRelation("a");
    EXPECT_NE(a, nullptr);
    const auto& clauses = a->getClauses();
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

}  // end namespace test
}  // end namespace souffle
