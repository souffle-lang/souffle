/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_condition_equal_clone_test.cpp
 *
 * Tests equal and clone function of RamCondition classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "BinaryConstraintOps.h"
#include "RelationTag.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamRelation.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

namespace test {

TEST(RamTrue, CloneAndEquals) {
    RamTrue a;
    RamTrue b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamTrue* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamFalse, CloneAndEquals) {
    RamFalse a;
    RamFalse b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamFalse* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamConjunction, CloneAndEquals) {
    // true /\ false
    auto a = std::make_unique<RamConjunction>(std::make_unique<RamTrue>(), std::make_unique<RamFalse>());
    auto b = std::make_unique<RamConjunction>(std::make_unique<RamTrue>(), std::make_unique<RamFalse>());
    EXPECT_EQ(*a, *b);
    EXPECT_NE(a, b);

    std::unique_ptr<RamConjunction> c(a->clone());
    EXPECT_EQ(*a, *c);
    EXPECT_NE(a, c);

    // true /\ (false /\ true)
    auto d = std::make_unique<RamConjunction>(std::make_unique<RamTrue>(),
            std::make_unique<RamConjunction>(std::make_unique<RamFalse>(), std::make_unique<RamTrue>()));
    auto e = std::make_unique<RamConjunction>(std::make_unique<RamTrue>(),
            std::make_unique<RamConjunction>(std::make_unique<RamFalse>(), std::make_unique<RamTrue>()));
    EXPECT_EQ(*d, *e);
    EXPECT_NE(d, e);

    std::unique_ptr<RamConjunction> f(d->clone());
    EXPECT_EQ(*d, *f);
    EXPECT_NE(d, f);

    // (true /\ false) /\ (true /\ (false /\ true))
    auto a_conj_d = std::make_unique<RamConjunction>(std::move(a), std::move(d));
    auto b_conj_e = std::make_unique<RamConjunction>(std::move(b), std::move(e));
    EXPECT_EQ(*a_conj_d, *b_conj_e);
    EXPECT_NE(a_conj_d, b_conj_e);

    auto c_conj_f = std::make_unique<RamConjunction>(std::move(c), std::move(f));
    EXPECT_EQ(*c_conj_f, *a_conj_d);
    EXPECT_EQ(*c_conj_f, *b_conj_e);
    EXPECT_NE(c_conj_f, a_conj_d);
    EXPECT_NE(c_conj_f, b_conj_e);

    std::unique_ptr<RamConjunction> a_conj_d_copy(a_conj_d->clone());
    EXPECT_EQ(*a_conj_d, *a_conj_d_copy);
    EXPECT_NE(a_conj_d, a_conj_d_copy);
}

TEST(RamNegation, CloneAndEquals) {
    auto a = std::make_unique<RamTrue>();
    auto neg_a = std::make_unique<RamNegation>(std::move(a));
    auto b = std::make_unique<RamTrue>();
    auto neg_b = std::make_unique<RamNegation>(std::move(b));
    EXPECT_EQ(*neg_a, *neg_b);
    EXPECT_NE(neg_a, neg_b);

    auto c = std::make_unique<RamFalse>();
    auto neg_neg_c = std::make_unique<RamNegation>(std::make_unique<RamNegation>(std::move(c)));
    auto d = std::make_unique<RamFalse>();
    auto neg_neg_d = std::make_unique<RamNegation>(std::make_unique<RamNegation>(std::move(d)));
    EXPECT_EQ(*neg_neg_c, *neg_neg_d);
    EXPECT_NE(neg_neg_c, neg_neg_d);
}

TEST(RamConstraint, CloneAndEquals) {
    // constraint t0.1 = t1.0
    std::unique_ptr<RamExpression> a_lhs(new RamTupleElement(0, 1));
    std::unique_ptr<RamExpression> a_rhs(new RamTupleElement(1, 0));
    std::unique_ptr<RamConstraint> a(
            new RamConstraint(BinaryConstraintOp::EQ, std::move(a_lhs), std::move(a_rhs)));
    std::unique_ptr<RamExpression> b_lhs(new RamTupleElement(0, 1));
    std::unique_ptr<RamExpression> b_rhs(new RamTupleElement(1, 0));
    std::unique_ptr<RamConstraint> b(
            new RamConstraint(BinaryConstraintOp::EQ, std::move(b_lhs), std::move(b_rhs)));
    EXPECT_EQ(*a, *b);
    EXPECT_NE(a, b);

    std::unique_ptr<RamConstraint> c(a->clone());
    EXPECT_EQ(*a, *c);
    EXPECT_EQ(*b, *c);
    EXPECT_NE(a, c);
    EXPECT_NE(b, c);

    // constraint t2.0 >= 5
    std::unique_ptr<RamExpression> d_lhs(new RamTupleElement(2, 0));
    std::unique_ptr<RamExpression> d_rhs(new RamSignedConstant(5));
    std::unique_ptr<RamConstraint> d(
            new RamConstraint(BinaryConstraintOp::EQ, std::move(d_lhs), std::move(d_rhs)));
    std::unique_ptr<RamExpression> e_lhs(new RamTupleElement(2, 0));
    std::unique_ptr<RamExpression> e_rhs(new RamSignedConstant(5));
    std::unique_ptr<RamConstraint> e(
            new RamConstraint(BinaryConstraintOp::EQ, std::move(e_lhs), std::move(e_rhs)));
    EXPECT_EQ(*d, *e);
    EXPECT_NE(d, e);

    std::unique_ptr<RamConstraint> f(d->clone());
    EXPECT_EQ(*d, *f);
    EXPECT_EQ(*e, *f);
    EXPECT_NE(d, f);
    EXPECT_NE(e, f);
}

TEST(RamExistenceCheck, CloneAndEquals) {
    // N(1) in relation N(x:number)
    RamRelation N("N", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    std::vector<std::unique_ptr<RamExpression>> tuple_a;
    tuple_a.emplace_back(new RamSignedConstant(1));
    RamExistenceCheck a(std::make_unique<RamRelationReference>(&N), std::move(tuple_a));
    std::vector<std::unique_ptr<RamExpression>> tuple_b;
    tuple_b.emplace_back(new RamSignedConstant(1));
    RamExistenceCheck b(std::make_unique<RamRelationReference>(&N), std::move(tuple_b));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamExistenceCheck* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_EQ(b, *c);
    EXPECT_NE(&a, c);
    EXPECT_NE(&b, c);

    delete c;

    // edge(1,2) in relation edge(x:number,y:number)
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::BRIE);
    std::vector<std::unique_ptr<RamExpression>> tuple_d;
    tuple_d.emplace_back(new RamSignedConstant(1));
    tuple_d.emplace_back(new RamSignedConstant(2));
    RamExistenceCheck d(std::make_unique<RamRelationReference>(&edge), std::move(tuple_d));
    std::vector<std::unique_ptr<RamExpression>> tuple_e;
    tuple_e.emplace_back(new RamSignedConstant(1));
    tuple_e.emplace_back(new RamSignedConstant(2));
    RamExistenceCheck e(std::make_unique<RamRelationReference>(&edge), std::move(tuple_e));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamExistenceCheck* f = d.clone();
    EXPECT_EQ(d, *f);
    EXPECT_EQ(e, *f);
    EXPECT_NE(&d, f);
    EXPECT_NE(&e, f);

    delete f;
}

TEST(RamProvenanceExistCheck, CloneAndEquals) {
    RamRelation N("N", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    std::vector<std::unique_ptr<RamExpression>> tuple_a;
    tuple_a.emplace_back(new RamSignedConstant(1));
    RamExistenceCheck a(std::make_unique<RamRelationReference>(&N), std::move(tuple_a));
    std::vector<std::unique_ptr<RamExpression>> tuple_b;
    tuple_b.emplace_back(new RamSignedConstant(1));
    RamExistenceCheck b(std::make_unique<RamRelationReference>(&N), std::move(tuple_b));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamExistenceCheck* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_EQ(b, *c);
    EXPECT_NE(&a, c);
    EXPECT_NE(&b, c);

    delete c;

    // address(state:symbol, postCode:number, street:symbol)
    RamRelation address("address", 3, 1, {"state", "postCode", "street"}, {"s", "i", "s"},
            RelationRepresentation::DEFAULT);
    std::vector<std::unique_ptr<RamExpression>> tuple_d;
    tuple_d.emplace_back(new RamSignedConstant(0));
    tuple_d.emplace_back(new RamSignedConstant(2000));
    tuple_d.emplace_back(new RamSignedConstant(0));
    RamProvenanceExistenceCheck d(std::make_unique<RamRelationReference>(&address), std::move(tuple_d));
    std::vector<std::unique_ptr<RamExpression>> tuple_e;
    tuple_e.emplace_back(new RamSignedConstant(0));
    tuple_e.emplace_back(new RamSignedConstant(2000));
    tuple_e.emplace_back(new RamSignedConstant(0));
    RamProvenanceExistenceCheck e(std::make_unique<RamRelationReference>(&address), std::move(tuple_e));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamProvenanceExistenceCheck* f = d.clone();
    EXPECT_EQ(d, *f);
    EXPECT_EQ(e, *f);
    EXPECT_NE(&d, f);
    EXPECT_NE(&e, f);

    delete f;
}

TEST(RamEmptinessCheck, CloneAndEquals) {
    // Check A(x:number)
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamEmptinessCheck a(std::make_unique<RamRelationReference>(&A));
    RamEmptinessCheck b(std::make_unique<RamRelationReference>(&A));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);
    RamEmptinessCheck* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_EQ(b, *c);
    EXPECT_NE(&a, c);
    EXPECT_NE(&b, c);
    delete c;
}

}  // end namespace test
}  // end namespace souffle
