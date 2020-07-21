/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_expression_equal_clone_test.cpp
 *
 * Tests equal and clone function of RamExpression classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "FunctorOps.h"
#include "RamTypes.h"
#include "ram/RamExpression.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

namespace test {

TEST(RamIntrinsicOperator, CloneAndEquals) {
    // ADD(number(1), number(2))
    std::vector<std::unique_ptr<RamExpression>> a_args;
    a_args.emplace_back(new RamSignedConstant(1));
    a_args.emplace_back(new RamSignedConstant(2));
    RamIntrinsicOperator a(FunctorOp::ADD, std::move(a_args));

    std::vector<std::unique_ptr<RamExpression>> b_args;
    b_args.emplace_back(new RamSignedConstant(1));
    b_args.emplace_back(new RamSignedConstant(2));
    RamIntrinsicOperator b(FunctorOp::ADD, std::move(b_args));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamIntrinsicOperator* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;

    // NEG(number(1))
    std::vector<std::unique_ptr<RamExpression>> d_args;
    d_args.emplace_back(new RamSignedConstant(1));
    RamIntrinsicOperator d(FunctorOp::NEG, std::move(d_args));

    std::vector<std::unique_ptr<RamExpression>> e_args;
    e_args.emplace_back(new RamSignedConstant(1));
    RamIntrinsicOperator e(FunctorOp::NEG, std::move(e_args));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamIntrinsicOperator* dClone = d.clone();
    EXPECT_EQ(d, *dClone);
    EXPECT_NE(&d, dClone);
    delete dClone;
}

TEST(RamUserDefinedOperator, CloneAndEquals) {
    // define binary functor NE check if two RamExpressions are not equal
    // NE(number(1), number(10))
    std::vector<std::unique_ptr<RamExpression>> a_args;
    a_args.emplace_back(new RamSignedConstant(1));
    a_args.emplace_back(new RamSignedConstant(10));
    RamUserDefinedOperator a(
            "NE", {TypeAttribute::Signed, TypeAttribute::Signed}, TypeAttribute::Signed, std::move(a_args));

    std::vector<std::unique_ptr<RamExpression>> b_args;
    b_args.emplace_back(new RamSignedConstant(1));
    b_args.emplace_back(new RamSignedConstant(10));
    RamUserDefinedOperator b(
            "NE", {TypeAttribute::Signed, TypeAttribute::Signed}, TypeAttribute::Signed, std::move(b_args));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamUserDefinedOperator* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(RamTupleElement, CloneAndEquals) {
    // t0.1
    RamTupleElement a(0, 1);
    RamTupleElement b(0, 1);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamTupleElement* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(RamSignedConstant, CloneAndEquals) {
    // number(5)
    RamSignedConstant a(5);
    RamSignedConstant b(5);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamSignedConstant* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(RamAutoIncrement, CloneAndEquals) {
    RamAutoIncrement a;
    RamAutoIncrement b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamAutoIncrement* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(RamUndefValue, CloneAndEquals) {
    RamUndefValue a;
    RamUndefValue b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamUndefValue* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(RamPackRecord, CloneAndEquals) {
    // {number{10), number(5), ⊥, ⊥}
    std::vector<std::unique_ptr<RamExpression>> a_args;
    a_args.emplace_back(new RamSignedConstant(10));
    a_args.emplace_back(new RamSignedConstant(5));
    a_args.emplace_back(new RamUndefValue);
    a_args.emplace_back(new RamUndefValue);
    RamPackRecord a(std::move(a_args));

    std::vector<std::unique_ptr<RamExpression>> b_args;
    b_args.emplace_back(new RamSignedConstant(10));
    b_args.emplace_back(new RamSignedConstant(5));
    b_args.emplace_back(new RamUndefValue);
    b_args.emplace_back(new RamUndefValue);
    RamPackRecord b(std::move(b_args));

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamPackRecord* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;

    // {⊥, {argument(1), number(0)}, t1.3}
    std::vector<std::unique_ptr<RamExpression>> d_args;
    d_args.emplace_back(new RamUndefValue);
    std::vector<std::unique_ptr<RamExpression>> d_record;
    d_record.emplace_back(new RamSubroutineArgument(1));
    d_record.emplace_back(new RamSignedConstant(5));
    d_args.emplace_back(new RamPackRecord(std::move(d_record)));
    d_args.emplace_back(new RamTupleElement(1, 3));
    RamPackRecord d(std::move(d_args));

    std::vector<std::unique_ptr<RamExpression>> e_args;
    e_args.emplace_back(new RamUndefValue);
    std::vector<std::unique_ptr<RamExpression>> e_record;
    e_record.emplace_back(new RamSubroutineArgument(1));
    e_record.emplace_back(new RamSignedConstant(5));
    e_args.emplace_back(new RamPackRecord(std::move(e_record)));
    e_args.emplace_back(new RamTupleElement(1, 3));
    RamPackRecord e(std::move(e_args));

    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamPackRecord* dClone = d.clone();
    EXPECT_EQ(d, *dClone);
    EXPECT_NE(&d, dClone);
    delete dClone;
}

TEST(RamSubrountineArgument, CloneAndEquals) {
    RamSubroutineArgument a(2);
    RamSubroutineArgument b(2);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamSubroutineArgument* aClone = a.clone();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}
}  // end namespace test
}  // end namespace souffle
