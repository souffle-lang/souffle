/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ast_print_test.cpp
 *
 * Tests souffle's AST program.
 *
 ***********************************************************************/

#include "AstArgument.h"
#include "AstLiteral.h"
#include "AstProgram.h"
#include "AstTranslationUnit.h"
#include "ParserDriver.h"
#include "test.h"

namespace souffle::test {

inline std::unique_ptr<AstTranslationUnit> makeATU(std::string program = ".decl A,B,C(x:number)") {
    ErrorReport e;
    DebugReport d;
    return ParserDriver::parseTranslationUnit(program, e, d);
}

inline std::unique_ptr<AstTranslationUnit> makePrintedATU(std::unique_ptr<AstTranslationUnit>& tu) {
    std::stringstream ss;
    tu->getProgram()->print(ss);
    return makeATU(ss.str());
}

inline std::unique_ptr<AstClause> makeClauseA(std::unique_ptr<AstArgument> headArgument) {
    auto headAtom = std::make_unique<AstAtom>("A");
    headAtom->addArgument(std::move(headArgument));
    auto clause = std::make_unique<AstClause>();
    clause->setHead(std::move(headAtom));
    return clause;
}

TEST(AstPrint, NilConstant) {
    auto testArgument = std::make_unique<AstNilConstant>();

    auto tu1 = makeATU();
    tu1->getProgram()->appendClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, NumberConstant) {
    auto testArgument = std::make_unique<AstNumberConstant>(2);

    auto tu1 = makeATU();
    tu1->getProgram()->appendClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, StringConstant) {
    ErrorReport e;
    DebugReport d;
    auto testArgument = std::make_unique<AstStringConstant>("test string");

    auto tu1 = ParserDriver::parseTranslationUnit(".decl A,B,C(x:number)", e, d);
    tu1->getProgram()->appendClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, Variable) {
    auto testArgument = std::make_unique<AstVariable>("testVar");

    auto tu1 = makeATU();
    tu1->getProgram()->appendClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, UnnamedVariable) {
    auto testArgument = std::make_unique<AstUnnamedVariable>();

    auto tu1 = makeATU();
    tu1->getProgram()->appendClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, Counter) {
    auto testArgument = std::make_unique<AstCounter>();

    auto tu1 = makeATU();
    tu1->getProgram()->appendClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, AggregatorMin) {
    auto atom = std::make_unique<AstAtom>("B");
    atom->addArgument(std::make_unique<AstVariable>("x"));
    auto min = std::make_unique<AstAggregator>(AstAggregator::min);
    min->setTargetExpression(std::make_unique<AstVariable>("x"));
    min->addBodyLiteral(std::move(atom));

    auto tu1 = makeATU();
    auto* prog1 = tu1->getProgram();
    prog1->appendClause(makeClauseA(std::move(min)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, AggregatorMax) {
    auto atom = std::make_unique<AstAtom>("B");
    atom->addArgument(std::make_unique<AstVariable>("x"));
    auto max = std::make_unique<AstAggregator>(AstAggregator::max);
    max->setTargetExpression(std::make_unique<AstVariable>("x"));
    max->addBodyLiteral(std::move(atom));

    auto tu1 = makeATU();
    auto* prog1 = tu1->getProgram();
    prog1->appendClause(makeClauseA(std::move(max)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, AggregatorCount) {
    auto atom = std::make_unique<AstAtom>("B");
    atom->addArgument(std::make_unique<AstVariable>("x"));
    auto count = std::make_unique<AstAggregator>(AstAggregator::count);
    count->addBodyLiteral(std::move(atom));

    auto tu1 = makeATU();
    auto* prog1 = tu1->getProgram();
    prog1->appendClause(makeClauseA(std::move(count)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}

TEST(AstPrint, AggregatorSum) {
    auto atom = std::make_unique<AstAtom>("B");
    atom->addArgument(std::make_unique<AstVariable>("x"));
    auto sum = std::make_unique<AstAggregator>(AstAggregator::sum);
    sum->setTargetExpression(std::make_unique<AstVariable>("x"));
    sum->addBodyLiteral(std::move(atom));

    auto tu1 = makeATU();
    auto* prog1 = tu1->getProgram();
    prog1->appendClause(makeClauseA(std::move(sum)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(*tu1->getProgram(), *tu2->getProgram());
}
}  // end namespace souffle::test
