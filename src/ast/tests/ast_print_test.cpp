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

#include "tests/test.h"

#include "AggregateOp.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Counter.h"
#include "ast/Literal.h"
#include "ast/NilConstant.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/StringConstant.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "parser/ParserDriver.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::test {

inline Own<TranslationUnit> makeATU(std::string program = ".decl A,B,C(x:number)") {
    ErrorReport e;
    DebugReport d;
    return ParserDriver::parseTranslationUnit(program, e, d);
}

inline Own<TranslationUnit> makePrintedATU(Own<TranslationUnit>& tu) {
    std::stringstream ss;
    ss << tu->getProgram();
    return makeATU(ss.str());
}

inline Own<Clause> makeClauseA(Own<Argument> headArgument) {
    auto clause = mk<Clause>("A");
    auto headAtom = clause->getHead();
    headAtom->addArgument(std::move(headArgument));
    return clause;
}

TEST(AstPrint, NilConstant) {
    auto testArgument = mk<NilConstant>();

    auto tu1 = makeATU();
    tu1->getProgram().addClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, NumberConstant) {
    auto testArgument = mk<NumericConstant>("2");

    EXPECT_EQ(testArgument, testArgument);

    auto tu1 = makeATU();
    tu1->getProgram().addClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, StringConstant) {
    ErrorReport e;
    DebugReport d;
    auto testArgument = mk<StringConstant>("test string");

    auto tu1 = ParserDriver::parseTranslationUnit(".decl A,B,C(x:number)", e, d);
    tu1->getProgram().addClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, Variable) {
    auto testArgument = mk<Variable>("testVar");

    auto tu1 = makeATU();
    tu1->getProgram().addClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, UnnamedVariable) {
    auto testArgument = mk<UnnamedVariable>();

    auto tu1 = makeATU();
    tu1->getProgram().addClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, Counter) {
    auto testArgument = mk<Counter>();

    auto tu1 = makeATU();
    tu1->getProgram().addClause(makeClauseA(std::move(testArgument)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, AggregatorMin) {
    auto atom = mk<Atom>("B");
    atom->addArgument(mk<Variable>("x"));
    auto min = mk<Aggregator>(AggregateOp::MIN, mk<Variable>("x"));

    VecOwn<Literal> body;
    body.push_back(mk<Atom>("B"));

    min->setBodyLiterals(std::move(body));

    auto tu1 = makeATU();
    auto& prog1 = tu1->getProgram();
    prog1.addClause(makeClauseA(std::move(min)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, AggregatorMax) {
    auto atom = mk<Atom>("B");
    atom->addArgument(mk<Variable>("x"));
    auto max = mk<Aggregator>(AggregateOp::MAX, mk<Variable>("x"));

    VecOwn<Literal> body;
    body.push_back(std::move(atom));
    max->setBodyLiterals(std::move(body));

    auto tu1 = makeATU();
    auto& prog1 = tu1->getProgram();
    prog1.addClause(makeClauseA(std::move(max)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, AggregatorCount) {
    auto atom = mk<Atom>("B");
    atom->addArgument(mk<Variable>("x"));
    auto count = mk<Aggregator>(AggregateOp::COUNT);

    VecOwn<Literal> body;
    body.push_back(std::move(atom));
    count->setBodyLiterals(std::move(body));

    auto tu1 = makeATU();
    auto& prog1 = tu1->getProgram();
    prog1.addClause(makeClauseA(std::move(count)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

TEST(AstPrint, AggregatorSum) {
    auto atom = mk<Atom>("B");
    atom->addArgument(mk<Variable>("x"));
    auto sum = mk<Aggregator>(AggregateOp::SUM, mk<Variable>("x"));

    VecOwn<Literal> body;
    body.push_back(std::move(atom));
    sum->setBodyLiterals(std::move(body));

    auto tu1 = makeATU();
    auto& prog1 = tu1->getProgram();
    prog1.addClause(makeClauseA(std::move(sum)));
    auto tu2 = makePrintedATU(tu1);
    EXPECT_EQ(tu1->getProgram(), tu2->getProgram());
}

}  // namespace souffle::ast::test
