/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_condition_equal_cloning_test.cpp
 *
 * Tests equal and cloning function of Condition classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "RelationTag.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/False.h"
#include "ram/Negation.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Relation.h"
#include "ram/SignedConstant.h"
#include "ram/True.h"
#include "ram/TupleElement.h"
#include "souffle/BinaryConstraintOps.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

namespace test {

TEST(True, CloneAndEquals) {
    True a;
    True b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    True* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(False, CloneAndEquals) {
    False a;
    False b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    False* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(Conjunction, CloneAndEquals) {
    // true /\ false
    auto a = mk<Conjunction>(mk<True>(), mk<False>());
    auto b = mk<Conjunction>(mk<True>(), mk<False>());
    EXPECT_EQ(*a, *b);
    EXPECT_NE(a, b);

    Own<Conjunction> c(a->cloning());
    EXPECT_EQ(*a, *c);
    EXPECT_NE(a, c);

    // true /\ (false /\ true)
    auto d = mk<Conjunction>(mk<True>(), mk<Conjunction>(mk<False>(), mk<True>()));
    auto e = mk<Conjunction>(mk<True>(), mk<Conjunction>(mk<False>(), mk<True>()));
    EXPECT_EQ(*d, *e);
    EXPECT_NE(d, e);

    Own<Conjunction> f(d->cloning());
    EXPECT_EQ(*d, *f);
    EXPECT_NE(d, f);

    // (true /\ false) /\ (true /\ (false /\ true))
    auto a_conj_d = mk<Conjunction>(std::move(a), std::move(d));
    auto b_conj_e = mk<Conjunction>(std::move(b), std::move(e));
    EXPECT_EQ(*a_conj_d, *b_conj_e);
    EXPECT_NE(a_conj_d, b_conj_e);

    auto c_conj_f = mk<Conjunction>(std::move(c), std::move(f));
    EXPECT_EQ(*c_conj_f, *a_conj_d);
    EXPECT_EQ(*c_conj_f, *b_conj_e);
    EXPECT_NE(c_conj_f, a_conj_d);
    EXPECT_NE(c_conj_f, b_conj_e);

    Own<Conjunction> a_conj_d_copy(a_conj_d->cloning());
    EXPECT_EQ(*a_conj_d, *a_conj_d_copy);
    EXPECT_NE(a_conj_d, a_conj_d_copy);
}

TEST(Negation, CloneAndEquals) {
    auto a = mk<True>();
    auto neg_a = mk<Negation>(std::move(a));
    auto b = mk<True>();
    auto neg_b = mk<Negation>(std::move(b));
    EXPECT_EQ(*neg_a, *neg_b);
    EXPECT_NE(neg_a, neg_b);

    auto c = mk<False>();
    auto neg_neg_c = mk<Negation>(mk<Negation>(std::move(c)));
    auto d = mk<False>();
    auto neg_neg_d = mk<Negation>(mk<Negation>(std::move(d)));
    EXPECT_EQ(*neg_neg_c, *neg_neg_d);
    EXPECT_NE(neg_neg_c, neg_neg_d);
}

TEST(Constraint, CloneAndEquals) {
    // constraint t0.1 = t1.0
    Own<Expression> a_lhs(new TupleElement(0, 1));
    Own<Expression> a_rhs(new TupleElement(1, 0));
    Own<Constraint> a(new Constraint(BinaryConstraintOp::EQ, std::move(a_lhs), std::move(a_rhs)));
    Own<Expression> b_lhs(new TupleElement(0, 1));
    Own<Expression> b_rhs(new TupleElement(1, 0));
    Own<Constraint> b(new Constraint(BinaryConstraintOp::EQ, std::move(b_lhs), std::move(b_rhs)));
    EXPECT_EQ(*a, *b);
    EXPECT_NE(a, b);

    Own<Constraint> c(a->cloning());
    EXPECT_EQ(*a, *c);
    EXPECT_EQ(*b, *c);
    EXPECT_NE(a, c);
    EXPECT_NE(b, c);

    // constraint t2.0 >= 5
    Own<Expression> d_lhs(new TupleElement(2, 0));
    Own<Expression> d_rhs(new SignedConstant(5));
    Own<Constraint> d(new Constraint(BinaryConstraintOp::EQ, std::move(d_lhs), std::move(d_rhs)));
    Own<Expression> e_lhs(new TupleElement(2, 0));
    Own<Expression> e_rhs(new SignedConstant(5));
    Own<Constraint> e(new Constraint(BinaryConstraintOp::EQ, std::move(e_lhs), std::move(e_rhs)));
    EXPECT_EQ(*d, *e);
    EXPECT_NE(d, e);

    Own<Constraint> f(d->cloning());
    EXPECT_EQ(*d, *f);
    EXPECT_EQ(*e, *f);
    EXPECT_NE(d, f);
    EXPECT_NE(e, f);
}

TEST(ExistenceCheck, CloneAndEquals) {
    // N(1) in relation N(x:number)
    Relation N("N", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    VecOwn<Expression> tuple_a;
    tuple_a.emplace_back(new SignedConstant(1));
    ExistenceCheck a("N", std::move(tuple_a));
    VecOwn<Expression> tuple_b;
    tuple_b.emplace_back(new SignedConstant(1));
    ExistenceCheck b("N", std::move(tuple_b));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    ExistenceCheck* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_EQ(b, *c);
    EXPECT_NE(&a, c);
    EXPECT_NE(&b, c);

    delete c;

    // edge(1,2) in relation edge(x:number,y:number)
    Relation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::BRIE);
    VecOwn<Expression> tuple_d;
    tuple_d.emplace_back(new SignedConstant(1));
    tuple_d.emplace_back(new SignedConstant(2));
    ExistenceCheck d("edge", std::move(tuple_d));
    VecOwn<Expression> tuple_e;
    tuple_e.emplace_back(new SignedConstant(1));
    tuple_e.emplace_back(new SignedConstant(2));
    ExistenceCheck e("edge", std::move(tuple_e));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    ExistenceCheck* f = d.cloning();
    EXPECT_EQ(d, *f);
    EXPECT_EQ(e, *f);
    EXPECT_NE(&d, f);
    EXPECT_NE(&e, f);

    delete f;
}

TEST(RamProvenanceExistCheck, CloneAndEquals) {
    Relation N("N", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    VecOwn<Expression> tuple_a;
    tuple_a.emplace_back(new SignedConstant(1));
    ExistenceCheck a("N", std::move(tuple_a));
    VecOwn<Expression> tuple_b;
    tuple_b.emplace_back(new SignedConstant(1));
    ExistenceCheck b("N", std::move(tuple_b));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    ExistenceCheck* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_EQ(b, *c);
    EXPECT_NE(&a, c);
    EXPECT_NE(&b, c);

    delete c;

    // address(state:symbol, postCode:number, street:symbol)
    Relation address("address", 3, 1, {"state", "postCode", "street"}, {"s", "i", "s"},
            RelationRepresentation::DEFAULT);
    VecOwn<Expression> tuple_d;
    tuple_d.emplace_back(new SignedConstant(0));
    tuple_d.emplace_back(new SignedConstant(2000));
    tuple_d.emplace_back(new SignedConstant(0));
    ProvenanceExistenceCheck d("address", std::move(tuple_d));
    VecOwn<Expression> tuple_e;
    tuple_e.emplace_back(new SignedConstant(0));
    tuple_e.emplace_back(new SignedConstant(2000));
    tuple_e.emplace_back(new SignedConstant(0));
    ProvenanceExistenceCheck e("address", std::move(tuple_e));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    ProvenanceExistenceCheck* f = d.cloning();
    EXPECT_EQ(d, *f);
    EXPECT_EQ(e, *f);
    EXPECT_NE(&d, f);
    EXPECT_NE(&e, f);

    delete f;
}

TEST(EmptinessCheck, CloneAndEquals) {
    // Check A(x:number)
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    EmptinessCheck a("A");
    EmptinessCheck b("A");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);
    EmptinessCheck* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_EQ(b, *c);
    EXPECT_NE(&a, c);
    EXPECT_NE(&b, c);
    delete c;
}

}  // end namespace test
}  // namespace souffle::ram
