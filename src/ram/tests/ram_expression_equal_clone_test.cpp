/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_expression_equal_cloning_test.cpp
 *
 * Tests equal and cloning function of Expression classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "FunctorOps.h"
#include "ram/AutoIncrement.h"
#include "ram/Expression.h"
#include "ram/IntrinsicOperator.h"
#include "ram/IterationNumber.h"
#include "ram/PackRecord.h"
#include "ram/SignedConstant.h"
#include "ram/SubroutineArgument.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "ram/UserDefinedOperator.h"
#include "souffle/TypeAttribute.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

namespace test {

TEST(IntrinsicOperator, CloneAndEquals) {
    // ADD(number(1), number(2))
    VecOwn<Expression> a_args;
    a_args.emplace_back(new SignedConstant(1));
    a_args.emplace_back(new SignedConstant(2));
    IntrinsicOperator a(FunctorOp::ADD, std::move(a_args));

    VecOwn<Expression> b_args;
    b_args.emplace_back(new SignedConstant(1));
    b_args.emplace_back(new SignedConstant(2));
    IntrinsicOperator b(FunctorOp::ADD, std::move(b_args));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    IntrinsicOperator* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;

    // NEG(number(1))
    VecOwn<Expression> d_args;
    d_args.emplace_back(new SignedConstant(1));
    IntrinsicOperator d(FunctorOp::NEG, std::move(d_args));

    VecOwn<Expression> e_args;
    e_args.emplace_back(new SignedConstant(1));
    IntrinsicOperator e(FunctorOp::NEG, std::move(e_args));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    IntrinsicOperator* dClone = d.cloning();
    EXPECT_EQ(d, *dClone);
    EXPECT_NE(&d, dClone);
    delete dClone;
}

TEST(UserDefinedOperator, CloneAndEquals) {
    // define binary functor NE check if two Expressions are not equal
    // NE(number(1), number(10))
    VecOwn<Expression> a_args;
    a_args.emplace_back(new SignedConstant(1));
    a_args.emplace_back(new SignedConstant(10));
    UserDefinedOperator a("NE", {TypeAttribute::Signed, TypeAttribute::Signed}, TypeAttribute::Signed, false,
            std::move(a_args));

    VecOwn<Expression> b_args;
    b_args.emplace_back(new SignedConstant(1));
    b_args.emplace_back(new SignedConstant(10));
    UserDefinedOperator b("NE", {TypeAttribute::Signed, TypeAttribute::Signed}, TypeAttribute::Signed, false,
            std::move(b_args));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    UserDefinedOperator* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(TupleElement, CloneAndEquals) {
    // t0.1
    TupleElement a(0, 1);
    TupleElement b(0, 1);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    TupleElement* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(SignedConstant, CloneAndEquals) {
    // number(5)
    SignedConstant a(5);
    SignedConstant b(5);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    SignedConstant* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(AutoIncrement, CloneAndEquals) {
    AutoIncrement a;
    AutoIncrement b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    AutoIncrement* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(IterationNumber, CloneAndEquals) {
    IterationNumber a;
    IterationNumber b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    IterationNumber* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(UndefValue, CloneAndEquals) {
    UndefValue a;
    UndefValue b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    UndefValue* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}

TEST(PackRecord, CloneAndEquals) {
    // {number{10), number(5), ⊥, ⊥}
    VecOwn<Expression> a_args;
    a_args.emplace_back(new SignedConstant(10));
    a_args.emplace_back(new SignedConstant(5));
    a_args.emplace_back(new UndefValue);
    a_args.emplace_back(new UndefValue);
    PackRecord a(std::move(a_args));

    VecOwn<Expression> b_args;
    b_args.emplace_back(new SignedConstant(10));
    b_args.emplace_back(new SignedConstant(5));
    b_args.emplace_back(new UndefValue);
    b_args.emplace_back(new UndefValue);
    PackRecord b(std::move(b_args));

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    PackRecord* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;

    // {⊥, {argument(1), number(0)}, t1.3}
    VecOwn<Expression> d_args;
    d_args.emplace_back(new UndefValue);
    VecOwn<Expression> d_record;
    d_record.emplace_back(new SubroutineArgument(1));
    d_record.emplace_back(new SignedConstant(5));
    d_args.emplace_back(new PackRecord(std::move(d_record)));
    d_args.emplace_back(new TupleElement(1, 3));
    PackRecord d(std::move(d_args));

    VecOwn<Expression> e_args;
    e_args.emplace_back(new UndefValue);
    VecOwn<Expression> e_record;
    e_record.emplace_back(new SubroutineArgument(1));
    e_record.emplace_back(new SignedConstant(5));
    e_args.emplace_back(new PackRecord(std::move(e_record)));
    e_args.emplace_back(new TupleElement(1, 3));
    PackRecord e(std::move(e_args));

    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    PackRecord* dClone = d.cloning();
    EXPECT_EQ(d, *dClone);
    EXPECT_NE(&d, dClone);
    delete dClone;
}

TEST(RamSubrountineArgument, CloneAndEquals) {
    SubroutineArgument a(2);
    SubroutineArgument b(2);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    SubroutineArgument* aClone = a.cloning();
    EXPECT_EQ(a, *aClone);
    EXPECT_NE(&a, aClone);
    delete aClone;
}
}  // end namespace test
}  // namespace souffle::ram
