/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_arithmetic_test.cpp
 *
 * Tests arithmetic evaluation by the Interpreter.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "FunctorOps.h"
#include "Global.h"
#include "interpreter/Engine.h"
#include "ram/Expression.h"
#include "ram/IntrinsicOperator.h"
#include "ram/Program.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/SubroutineReturn.h"
#include "ram/TranslationUnit.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/RamTypes.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace souffle::interpreter::test {

using namespace ram;

#define TESTS_PER_OPERATION 20

/** Function to evaluate a single Expression. */
RamDomain evalExpression(Own<Expression> expression) {
    // Set up Program and translation unit
    VecOwn<Expression> returnValues;
    returnValues.emplace_back(std::move(expression));

    Global::config().set("jobs", "1");
    Own<Statement> query = mk<ram::Query>(mk<ram::SubroutineReturn>(std::move(returnValues)));
    std::map<std::string, Own<Statement>> subs;
    subs.insert(std::make_pair("test", std::move(query)));
    VecOwn<ram::Relation> rels;

    Own<Program> prog = mk<Program>(std::move(rels), mk<ram::Sequence>(), std::move(subs));

    ErrorReport errReport;
    DebugReport debugReport;

    TranslationUnit translationUnit(std::move(prog), errReport, debugReport);

    // configure and execute interpreter
    Own<Engine> interpreter = mk<Engine>(translationUnit);

    std::string name("test");
    std::vector<RamDomain> ret;

    interpreter->executeSubroutine(name, {}, ret);

    return ret.at(0);
}

RamDomain evalMultiArg(FunctorOp functor, VecOwn<Expression> args) {
    return evalExpression(mk<ram::IntrinsicOperator>(functor, std::move(args)));
}

/** Evaluate a single argument expression */
RamDomain evalUnary(FunctorOp functor, RamDomain arg1) {
    VecOwn<Expression> args;
    args.push_back(mk<SignedConstant>(arg1));

    return evalMultiArg(functor, std::move(args));
}

/** Evaluate a binary operator */
RamDomain evalBinary(FunctorOp functor, RamDomain arg1, RamDomain arg2) {
    VecOwn<Expression> args;
    args.push_back(mk<SignedConstant>(arg1));
    args.push_back(mk<SignedConstant>(arg2));

    return evalMultiArg(functor, std::move(args));
}

TEST(SignedConstant, ArithmeticEvaluation) {
    RamDomain num = 42;
    Own<Expression> expression = mk<SignedConstant>(num);
    RamDomain result = evalExpression(std::move(expression));
    EXPECT_EQ(result, num);
}

TEST(Unary, Neg) {
    for (RamDomain randomNumber : testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION)) {
        EXPECT_EQ(evalUnary(FunctorOp::NEG, randomNumber), -randomNumber);
    }
}

TEST(Unary, FloatNeg) {
    FunctorOp functor = FunctorOp::FNEG;

    for (auto randomNumber : testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION)) {
        auto result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(ramBitCast<RamFloat>(result), -randomNumber);
    }
}

TEST(Unary, BinaryNot) {
    FunctorOp functor = FunctorOp::BNOT;

    for (auto randomNumber : testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION)) {
        EXPECT_EQ(evalUnary(functor, randomNumber), ~randomNumber);
    }
}

TEST(Unary, UnsignedBinaryNot) {
    FunctorOp functor = FunctorOp::UBNOT;

    for (auto randomNumber : testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), ~randomNumber);
    }
}

TEST(Unary, LogicalNeg) {
    FunctorOp functor = FunctorOp::LNOT;

    for (auto randomNumber : testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION)) {
        EXPECT_EQ(evalUnary(functor, randomNumber), !randomNumber);
    }
}

TEST(Unary, UnsignedLogicalNeg) {
    FunctorOp functor = FunctorOp::ULNOT;

    for (auto randomNumber : testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), static_cast<RamUnsigned>(!randomNumber));
    }
}

TEST(Unary, SingedTpUnsigned) {
    FunctorOp functor = FunctorOp::I2U;

    for (auto randomNumber : testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, randomNumber);
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), static_cast<RamUnsigned>(randomNumber));
    }
}

TEST(Unary, UnsignedToSigned) {
    FunctorOp functor = FunctorOp::U2I;

    for (auto randomNumber : testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(result, static_cast<RamDomain>(randomNumber));
    }
}

TEST(Unary, SignedToFloat) {
    FunctorOp functor = FunctorOp::I2F;

    for (auto randomNumber : testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(ramBitCast<RamFloat>(result), static_cast<RamFloat>(randomNumber));
    }
}

TEST(Unary, FloatToSigned) {
    FunctorOp functor = FunctorOp::F2I;

    for (auto randomNumber : testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(result, static_cast<RamDomain>(randomNumber));
    }
}

TEST(Unary, UnsignedToFloat) {
    FunctorOp functor = FunctorOp::U2F;

    for (auto randomNumber : testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(ramBitCast<RamFloat>(result), static_cast<RamFloat>(randomNumber));
    }
}

TEST(Unary, FloatToUnsigned) {
    FunctorOp functor = FunctorOp::F2U;

    for (auto randomNumber : testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION)) {
        RamDomain result = evalUnary(functor, ramBitCast(randomNumber));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), static_cast<RamUnsigned>(randomNumber));
    }
}

TEST(Binary, SignedAdd) {
    FunctorOp functor = FunctorOp::ADD;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (std::size_t i = 0; i < TESTS_PER_OPERATION; ++i) {
        RamDomain arg1 = vecArg1[i];
        RamDomain arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 + arg2);
    }
}

TEST(Binary, UnsignedAdd) {
    FunctorOp functor = FunctorOp::UADD;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        RamUnsigned arg1 = vecArg1[i];
        RamUnsigned arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 + arg2);
    }
}

TEST(Binary, FloatAdd) {
    FunctorOp functor = FunctorOp::FADD;

    auto vecArg1 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamFloat>(result), arg1 + arg2);
    }
}

TEST(Binary, SignedSub) {
    FunctorOp functor = FunctorOp::SUB;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 - arg2);
    }
}

TEST(Binary, UnsignedSub) {
    FunctorOp functor = FunctorOp::USUB;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 - arg2);
    }
}

TEST(Binary, FloatSub) {
    FunctorOp functor = FunctorOp::FSUB;

    auto vecArg1 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamFloat>(result), arg1 - arg2);
    }
}

TEST(Binary, SignedMul) {
    FunctorOp functor = FunctorOp::MUL;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 * arg2);
    }
}

TEST(Binary, UnsignedMul) {
    FunctorOp functor = FunctorOp::UMUL;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 * arg2);
    }
}

TEST(Binary, FloatMul) {
    FunctorOp functor = FunctorOp::FMUL;

    auto vecArg1 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamFloat>(result), arg1 * arg2);
    }
}

TEST(Binary, SignedDiv) {
    FunctorOp functor = FunctorOp::DIV;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        if (arg2 != 0) {
            RamDomain result = evalBinary(functor, arg1, arg2);
            EXPECT_EQ(result, arg1 / arg2);
        }
    }
}

TEST(Binary, UnsignedDiv) {
    FunctorOp functor = FunctorOp::UDIV;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        if (arg2 != 0) {
            RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
            EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 / arg2);
        }
    }
}

TEST(Binary, FloatDiv) {
    FunctorOp functor = FunctorOp::FDIV;

    auto vecArg1 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        if (arg2 != 0) {
            RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
            EXPECT_EQ(ramBitCast<RamFloat>(result), arg1 / arg2);
        }
    }
}

TEST(Binary, SignedExp) {
    FunctorOp functor = FunctorOp::EXP;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, static_cast<RamDomain>(static_cast<int64_t>(std::pow(arg1, arg2))));
    }
}

TEST(Binary, UnsignedExp) {
    FunctorOp functor = FunctorOp::UEXP;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), static_cast<RamUnsigned>(std::pow(arg1, arg2)));
    }
}

TEST(Binary, FloatExp) {
    FunctorOp functor = FunctorOp::FEXP;

    auto vecArg1 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamFloat>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        auto result = ramBitCast<RamFloat>(evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2)));
        auto expected = static_cast<RamFloat>(std::pow(arg1, arg2));
        EXPECT_TRUE((std::isnan(result) && std::isnan(expected)) || result == expected);
    }
}

TEST(Binary, SignedMod) {
    FunctorOp functor = FunctorOp::MOD;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 % arg2);
    }
}

TEST(Binary, UnsignedMod) {
    FunctorOp functor = FunctorOp::UMOD;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 % arg2);
    }
}

TEST(Binary, SignedBinaryAnd) {
    FunctorOp functor = FunctorOp::BAND;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 & arg2);
    }
}

TEST(Binary, UnsignedBinaryAnd) {
    FunctorOp functor = FunctorOp::UBAND;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 & arg2);
    }
}

TEST(Binary, SignedBinaryOr) {
    FunctorOp functor = FunctorOp::BOR;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 | arg2);
    }
}

TEST(Binary, UnsignedBinaryOr) {
    FunctorOp functor = FunctorOp::UBOR;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 | arg2);
    }
}

TEST(Binary, SignedBinaryXor) {
    FunctorOp functor = FunctorOp::BXOR;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 ^ arg2);
    }
}

TEST(Binary, UnsignedBinaryXor) {
    FunctorOp functor = FunctorOp::UBXOR;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 ^ arg2);
    }
}

TEST(Binary, SignedLogicalAnd) {
    FunctorOp functor = FunctorOp::LAND;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 || arg2);
    }
}

TEST(Binary, UnsignedLogicalAnd) {
    FunctorOp functor = FunctorOp::ULAND;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 || arg2);
    }
}

TEST(Binary, SignedLogicalOr) {
    FunctorOp functor = FunctorOp::LOR;

    auto vecArg1 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamDomain>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, arg1, arg2);
        EXPECT_EQ(result, arg1 || arg2);
    }
}

TEST(Binary, UnsignedLogicalOr) {
    FunctorOp functor = FunctorOp::ULOR;

    auto vecArg1 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);
    auto vecArg2 = testutil::generateRandomVector<RamUnsigned>(TESTS_PER_OPERATION);

    for (int i = 0; i < TESTS_PER_OPERATION; ++i) {
        auto arg1 = vecArg1[i];
        auto arg2 = vecArg2[i];
        RamDomain result = evalBinary(functor, ramBitCast(arg1), ramBitCast(arg2));
        EXPECT_EQ(ramBitCast<RamUnsigned>(result), arg1 || arg2);
    }
}

TEST(MultiArg, Max) {
    FunctorOp functor = FunctorOp::MAX;
    VecOwn<Expression> args;

    for (RamDomain i = 0; i <= 50; ++i) {
        args.push_back(mk<SignedConstant>(i));
    }

    RamDomain result = evalMultiArg(functor, std::move(args));

    EXPECT_EQ(result, 50);
}

TEST(MultiArg, UnsignedMax) {
    FunctorOp functor = FunctorOp::UMAX;
    VecOwn<Expression> args;

    for (RamUnsigned i = 0; i <= 100; ++i) {
        args.push_back(mk<SignedConstant>(ramBitCast(i)));
    }

    RamDomain result = evalMultiArg(functor, std::move(args));

    EXPECT_EQ(ramBitCast<RamUnsigned>(result), 100);
}

TEST(MultiArg, FloatMax) {
    FunctorOp functor = FunctorOp::FMAX;
    VecOwn<Expression> args;

    for (RamDomain i = -100; i <= 100; ++i) {
        args.push_back(mk<SignedConstant>(ramBitCast(static_cast<RamFloat>(i))));
    }

    RamDomain result = evalMultiArg(functor, std::move(args));

    EXPECT_EQ(ramBitCast<RamFloat>(result), static_cast<RamFloat>(100));
}

TEST(MultiArg, Min) {
    FunctorOp functor = FunctorOp::MIN;
    VecOwn<Expression> args;

    for (RamDomain i = 0; i <= 50; ++i) {
        args.push_back(mk<SignedConstant>(i));
    }

    RamDomain result = evalMultiArg(functor, std::move(args));

    EXPECT_EQ(result, 0);
}

TEST(MultiArg, UnsignedMin) {
    FunctorOp functor = FunctorOp::UMIN;
    VecOwn<Expression> args;

    for (RamUnsigned i = 0; i <= 100; ++i) {
        args.push_back(mk<SignedConstant>(ramBitCast(i)));
    }

    RamDomain result = evalMultiArg(functor, std::move(args));

    EXPECT_EQ(ramBitCast<RamUnsigned>(result), 0);
}

TEST(MultiArg, FloatMin) {
    FunctorOp functor = FunctorOp::FMIN;
    VecOwn<Expression> args;

    for (RamDomain i = -100; i <= 100; ++i) {
        args.push_back(mk<SignedConstant>(ramBitCast(static_cast<RamFloat>(i))));
    }

    RamDomain result = evalMultiArg(functor, std::move(args));

    EXPECT_EQ(ramBitCast<RamFloat>(result), static_cast<RamFloat>(-100));
}

}  // namespace souffle::interpreter::test
