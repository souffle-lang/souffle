/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file type_system_test.h
 *
 * Tests souffle's type system operations.
 *
 ***********************************************************************/

#include "AstTransforms.h"
#include "AstTranslationUnit.h"
#include "AstTypeAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "ParserDriver.h"
#include "test.h"

namespace souffle {

namespace test {

TEST(Ast, CloneAndEquals) {
    // TODO: add test for records
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;

    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .decl r(a:number,b:number,c:number,d:number)
                 r(X,Y,Z,W) :- a(X), 10 = Y, Y = Z, 8 + W = 12 + 14. 
            )",
            sym, e, d);
    AstProgram& program = *tu->getProgram();

    EXPECT_EQ(program, program);

    // clone and check for equality
    AstProgram* clone = program.clone();
    EXPECT_NE(clone, &program);
    EXPECT_EQ(*clone, program);
    delete clone;
}

TEST(AstUtils, Const) {
    // TODO: add test for records

    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                 .decl r(a:number,b:number,c:number,d:number)
                 r(X,Y,Z,W) :- a(X), 10 = Y, Y = Z, 8 + W = 12 + 14. 
            )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    AstClause* clause = program.getRelation("r")->getClause(0);

    // check construction
    EXPECT_EQ("r(X,Y,Z,W) :- \n   a(X),\n   10 = Y,\n   Y = Z,\n   (8+W) = (12+14).", toString(*clause));

    // obtain analyse constness
    auto isConst = getConstTerms(*clause);

    // check selected sub-terms
    auto head = clause->getHead();
    EXPECT_FALSE(isConst[head->getArgument(0)]);  // X
    EXPECT_TRUE(isConst[head->getArgument(1)]);   // Y
    EXPECT_TRUE(isConst[head->getArgument(2)]);   // Z
    EXPECT_TRUE(isConst[head->getArgument(3)]);   // W
}

TEST(AstUtils, Grounded) {
    // create an example clause:
    AstClause* clause = new AstClause();

    // something like:
    //   r(X,Y,Z) :- a(X), X = Y, !b(Z).

    // r(X,Y,Z)
    AstAtom* head = new AstAtom("r");
    head->addArgument(std::unique_ptr<AstArgument>(new AstVariable("X")));
    head->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Y")));
    head->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Z")));
    clause->setHead(std::unique_ptr<AstAtom>(head));

    // a(X)
    AstAtom* a = new AstAtom("a");
    a->addArgument(std::unique_ptr<AstArgument>(new AstVariable("X")));
    clause->addToBody(std::unique_ptr<AstLiteral>(a));

    // X = Y
    AstLiteral* e1 = new AstConstraint("=", std::unique_ptr<AstArgument>(new AstVariable("X")),
            std::unique_ptr<AstArgument>(new AstVariable("Y")));
    clause->addToBody(std::unique_ptr<AstLiteral>(e1));

    // !b(Z)
    AstAtom* b = new AstAtom("b");
    b->addArgument(std::unique_ptr<AstArgument>(new AstVariable("Z")));
    AstNegation* neg = new AstNegation(std::unique_ptr<AstAtom>(b));
    clause->addToBody(std::unique_ptr<AstLiteral>(neg));

    // check construction
    EXPECT_EQ("r(X,Y,Z) :- \n   a(X),\n   !b(Z),\n   X = Y.", toString(*clause));

    // obtain groundness
    auto isGrounded = getGroundedTerms(*clause);

    // check selected sub-terms
    EXPECT_TRUE(isGrounded[head->getArgument(0)]);   // X
    EXPECT_TRUE(isGrounded[head->getArgument(1)]);   // Y
    EXPECT_FALSE(isGrounded[head->getArgument(2)]);  // Z

    // done
    delete clause;
}

TEST(AstUtils, GroundedRecords) {
    SymbolTable sym;
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
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    auto clause = program.getRelation("s")->getClause(0);

    // check construction
    EXPECT_EQ("s(x) :- \n   r([x,y]).", toString(*clause));

    // obtain groundness
    auto isGrounded = getGroundedTerms(*clause);

    const AstAtom* s = clause->getHead();
    const AstAtom* r = dynamic_cast<const AstAtom*>(clause->getBodyLiteral(0));

    EXPECT_TRUE(s);
    EXPECT_TRUE(r);

    // check selected sub-terms
    EXPECT_TRUE(isGrounded[s->getArgument(0)]);
    EXPECT_TRUE(isGrounded[r->getArgument(0)]);
}

TEST(AstUtils, SimpleTypes) {
    SymbolTable sym;
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
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("a")->getClause(0);
    AstClause* b = program.getRelation("b")->getClause(0);
    AstClause* u = program.getRelation("u")->getClause(0);

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    auto getX = [](const AstClause* c) { return c->getHead()->getArgument(0); };

    EXPECT_EQ("{A}", toString(typeAnalysis->getTypes(getX(a))));
    EXPECT_EQ("{B}", toString(typeAnalysis->getTypes(getX(b))));
    EXPECT_EQ("{U}", toString(typeAnalysis->getTypes(getX(u))));

    AstClause* a1 = program.getRelation("a")->getClause(1);
    EXPECT_EQ("{}", toString(typeAnalysis->getTypes(getX(a1))));

    AstClause* a2 = program.getRelation("a")->getClause(2);
    EXPECT_EQ("{A}", toString(typeAnalysis->getTypes(getX(a2))));
}

TEST(AstUtils, NumericTypes) {
    SymbolTable sym;
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
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("a")->getClause(0);
    AstClause* b = program.getRelation("b")->getClause(0);
    AstClause* u = program.getRelation("u")->getClause(0);

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    auto getX = [](const AstClause* c) { return c->getHead()->getArgument(0); };

    EXPECT_EQ("{}", toString(typeAnalysis->getTypes(getX(a))));
    EXPECT_EQ("{B}", toString(typeAnalysis->getTypes(getX(b))));
    EXPECT_EQ("{U}", toString(typeAnalysis->getTypes(getX(u))));
}

TEST(AstUtils, SubtypeChain) {
    SymbolTable sym;
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
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("R4")->getClause(0);

    auto getX = [](const AstClause* c) { return c->getHead()->getArgument(0); };

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
    SymbolTable sym;
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
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("a")->getClause(0);
    AstClause* b = program.getRelation("b")->getClause(0);
    AstClause* u = program.getRelation("u")->getClause(0);

    auto typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    auto getX = [](const AstClause* c) { return c->getHead()->getArgument(0); };

    EXPECT_EQ("{A}", toString(typeAnalysis->getTypes(getX(a))));
    EXPECT_EQ("{B}", toString(typeAnalysis->getTypes(getX(b))));
    EXPECT_EQ("{U}", toString(typeAnalysis->getTypes(getX(u))));
}

TEST(AstUtils, NestedFunctions) {
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl r(x:D)
            
                r(x) :- r(y), x=cat(cat(x,x),x).
            )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("r")->getClause(0);

    auto getX = [](const AstClause* c) { return c->getHead()->getArgument(0); };

    // check proper type deduction
    EXPECT_EQ("{D}", toString(tu->getAnalysis<TypeAnalysis>()->getTypes(getX(a))));
}

TEST(AstUtils, GroundTermPropagation) {
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("p")->getClause(0);

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
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
               .type D
               .decl p(a:D,b:D)

               p(a,b) :- p(x,y), x = y, x = a, y = b.
           )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    // check types in clauses
    AstClause* a = program.getRelation("p")->getClause(0);

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   x = y,\n   x = a,\n   y = b.", toString(*a));

    std::unique_ptr<AstClause> res = ResolveAliasesTransformer::resolveAliases(*a);
    std::unique_ptr<AstClause> cleaned = ResolveAliasesTransformer::removeTrivialEquality(*res);

    EXPECT_EQ("p(b,b) :- \n   p(b,b),\n   b = b,\n   b = b,\n   b = b.", toString(*res));
    EXPECT_EQ("p(b,b) :- \n   p(b,b).", toString(*cleaned));
}

TEST(AstUtils, ResolveGroundedAliases) {
    // load some test program
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl p(a:D,b:D)

                p(a,b) :- p(x,y), r = [x,y], s = r, s = [w,v], [w,v] = [a,b].
            )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ("p(a,b) :- \n   p(x,y),\n   r = [x,y],\n   s = r,\n   s = [w,v],\n   [w,v] = [a,b].",
            toString(*program.getRelation("p")->getClause(0)));

    ResolveAliasesTransformer::resolveAliases(program);

    EXPECT_EQ("p(x,y) :- \n   p(x,y).", toString(*program.getRelation("p")->getClause(0)));
}

TEST(AstUtils, ResolveAliasesWithTermsInAtoms) {
    // load some test program
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D
                .decl p(a:D,b:D)

                p(x,c) :- p(x,b), p(b,c), c = b+1, x=c+2.
            )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ("p(x,c) :- \n   p(x,b),\n   p(b,c),\n   c = (b+1),\n   x = (c+2).",
            toString(*program.getRelation("p")->getClause(0)));

    ResolveAliasesTransformer::resolveAliases(program);

    EXPECT_EQ(
            "p(((b+1)+2),(b+1)) :- \n   p( _tmp_0,b),\n   p(b, _tmp_1),\n    _tmp_0 = ((b+1)+2),\n    _tmp_1 "
            "= (b+1).",
            toString(*program.getRelation("p")->getClause(0)));
}

TEST(AstUtils, RemoveRelationCopies) {
    SymbolTable sym;
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
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ(4, program.getRelations().size());

    RemoveRelationCopiesTransformer::removeRelationCopies(program);

    EXPECT_EQ(2, program.getRelations().size());
}

TEST(AstUtils, RemoveRelationCopiesOutput) {
    SymbolTable sym;
    ErrorReport e;
    DebugReport d;
    // load some test program
    std::unique_ptr<AstTranslationUnit> tu = ParserDriver::parseTranslationUnit(
            R"(
                .type D = number
                .decl a(a:D,b:D)
                .decl b(a:D,b:D)
                .decl c(a:D,b:D) output
                .decl d(a:D,b:D)

                a(1,2).
                b(x,y) :- a(x,y).
                c(x,y) :- b(x,y).

                d(x,y) :- b(x,y), c(y,x).

            )",
            sym, e, d);

    AstProgram& program = *tu->getProgram();

    EXPECT_EQ(4, program.getRelations().size());

    RemoveRelationCopiesTransformer::removeRelationCopies(program);

    EXPECT_EQ(3, program.getRelations().size());
}

}  // end namespace test
}  // end namespace souffle
