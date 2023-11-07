/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_statement_equal_cloning_test.cpp
 *
 * Tests equal and cloning function of Statement classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "FunctorOps.h"
#include "RelationTag.h"
#include "ram/Break.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/MergeExtend.h"
#include "ram/Negation.h"
#include "ram/Operation.h"
#include "ram/Parallel.h"
#include "ram/ParallelIfExists.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/SubroutineReturn.h"
#include "ram/Swap.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "souffle/BinaryConstraintOps.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

namespace test {

TEST(IO1, CloneAndEquals) {
    // IO A ()
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    std::map<std::string, std::string> ioEmptyA;
    std::map<std::string, std::string> ioEmptyB;
    IO a("A", std::move(ioEmptyA));
    IO b("A", std::move(ioEmptyB));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    IO* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(Clear, CloneAndEquals) {
    // CLEAR A
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    Clear a("A");
    Clear b("A");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Clear* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(MergeExtend, CloneAndEquals) {
    // MERGE B WITH A
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    Relation B("B", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    MergeExtend a("B", "A");
    MergeExtend b("B", "A");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    MergeExtend* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(Swap, CloneAndEquals) {
    // SWAP(A,B)
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    Relation B("B", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    Swap a("A", "B");
    Swap b("A", "B");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Swap* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(Query, CloneAndEquals) {
    Relation A("A", 3, 1, {"a", "b", "c"}, {"i", "s", "i"}, RelationRepresentation::DEFAULT);
    Relation B("B", 2, 1, {"a", "c"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    /*
     * QUERY
     *  FOR t0 IN A
     *   INSERT (t0.0, t0.2) INTO B
     */
    VecOwn<Expression> a_expressions;
    a_expressions.emplace_back(new TupleElement(0, 0));
    a_expressions.emplace_back(new TupleElement(0, 2));
    auto a_insert = mk<Insert>("B", std::move(a_expressions));
    auto a_scan = mk<Scan>("A", 0, std::move(a_insert), "");

    VecOwn<Expression> b_expressions;
    b_expressions.emplace_back(new TupleElement(0, 0));
    b_expressions.emplace_back(new TupleElement(0, 2));
    auto b_insert = mk<Insert>("B", std::move(b_expressions));
    auto b_scan = mk<Scan>("A", 0, std::move(b_insert), "");

    Query a(std::move(a_scan));
    Query b(std::move(b_scan));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Query* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;

    /*
     * QUERY
     *  PARALLEL IF EXISTS t1 IN A WHERE (t1.0 = 0)
     *   RETURN (t1.2)
     */
    VecOwn<Expression> d_return_value;
    d_return_value.emplace_back(new TupleElement(1, 0));
    auto d_return = mk<SubroutineReturn>(std::move(d_return_value));
    // condition t1.0 = 0
    auto d_cond = mk<Constraint>(BinaryConstraintOp::EQ, mk<TupleElement>(1, 0), mk<SignedConstant>(0));
    auto d_parallel_ifexists = mk<ParallelIfExists>("A", 1, std::move(d_cond), std::move(d_return), "");

    VecOwn<Expression> e_return_value;
    e_return_value.emplace_back(new TupleElement(1, 0));
    auto e_return = mk<SubroutineReturn>(std::move(e_return_value));
    // condition t1.0 = 0
    auto e_cond = mk<Constraint>(BinaryConstraintOp::EQ, mk<TupleElement>(1, 0), mk<SignedConstant>(0));
    auto e_parallel_ifexists = mk<ParallelIfExists>("A", 1, std::move(e_cond), std::move(e_return), "");
    Query d(std::move(d_parallel_ifexists));
    Query e(std::move(e_parallel_ifexists));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    Query* f = d.cloning();
    EXPECT_EQ(d, *f);
    EXPECT_NE(&d, f);
    delete f;
}

TEST(Sequence, CloneAndEquals) {
    // no statements in the sequence
    Sequence a;
    Sequence b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Sequence* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;

    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // one statement in the sequence
    // CLEAR A
    Sequence d(mk<Clear>("A"));
    Sequence e(mk<Clear>("A"));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    Sequence* f = d.cloning();
    EXPECT_EQ(d, *f);
    EXPECT_NE(&d, f);
    delete f;

    // multiple statements in the sequence
    // IO A ()
    // CLEAR A
    std::map<std::string, std::string> g_load_IODir;
    std::map<std::string, std::string> h_load_IODir;
    Sequence g(mk<IO>("A", std::move(g_load_IODir)), mk<Clear>("A"));
    Sequence h(mk<IO>("A", std::move(h_load_IODir)), mk<Clear>("A"));
    EXPECT_EQ(g, h);
    EXPECT_NE(&g, &h);

    Sequence* i = g.cloning();
    EXPECT_EQ(g, *i);
    EXPECT_NE(&g, i);
    delete i;
}

TEST(Parallel, CloneAndEquals) {
    Relation A("A", 3, 1, {"a", "b", "c"}, {"i", "s", "i"}, RelationRepresentation::DEFAULT);
    Relation B("B", 2, 1, {"a", "c"}, {"i", "i"}, RelationRepresentation::DEFAULT);

    /* PARALLEL
     *  QUERY
     *   FOR t0 IN A
     *    IF (t0.0 > 0)
     *     INSERT (t0.0, t0.2) INTO B
     * END PARALLEL
     * */

    VecOwn<Expression> a_expressions;
    a_expressions.emplace_back(new TupleElement(0, 0));
    a_expressions.emplace_back(new TupleElement(0, 2));
    auto a_insert = mk<Insert>("B", std::move(a_expressions));
    auto a_cond =
            mk<Filter>(mk<Constraint>(BinaryConstraintOp::GE, mk<TupleElement>(0, 0), mk<SignedConstant>(0)),
                    std::move(a_insert), "");
    auto a_scan = mk<Scan>("A", 0, std::move(a_cond), "");
    auto a_query = mk<Query>(std::move(a_scan));
    Parallel a(std::move(a_query));

    VecOwn<Expression> b_expressions;
    b_expressions.emplace_back(new TupleElement(0, 0));
    b_expressions.emplace_back(new TupleElement(0, 2));
    auto b_insert = mk<Insert>("B", std::move(b_expressions));
    auto b_cond =
            mk<Filter>(mk<Constraint>(BinaryConstraintOp::GE, mk<TupleElement>(0, 0), mk<SignedConstant>(0)),
                    std::move(b_insert), "");
    auto b_scan = mk<Scan>("A", 0, std::move(b_cond), "");
    auto b_query = mk<Query>(std::move(b_scan));
    Parallel b(std::move(b_query));

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Parallel* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}
TEST(Loop, CloneAndEquals) {
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    Relation B("B", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    /*
     * LOOP
     *  QUERY
     *   FOR t0 IN A
     *    IF t0.0 = 4 BREAK
     *    INSERT (t0.0) INTO B
     * END LOOP
     * */
    VecOwn<Expression> a_expressions;
    a_expressions.emplace_back(new TupleElement(0, 0));
    auto a_insert = mk<Insert>("B", std::move(a_expressions));
    auto a_break =
            mk<Break>(mk<Constraint>(BinaryConstraintOp::EQ, mk<TupleElement>(0, 0), mk<SignedConstant>(4)),
                    std::move(a_insert), "");
    auto a_scan = mk<Scan>("A", 0, std::move(a_break), "");
    auto a_query = mk<Query>(std::move(a_scan));
    Loop a(std::move(a_query));

    VecOwn<Expression> b_expressions;
    b_expressions.emplace_back(new TupleElement(0, 0));
    auto b_insert = mk<Insert>("B", std::move(b_expressions));
    auto b_break =
            mk<Break>(mk<Constraint>(BinaryConstraintOp::EQ, mk<TupleElement>(0, 0), mk<SignedConstant>(4)),
                    std::move(b_insert), "");
    auto b_scan = mk<Scan>("A", 0, std::move(b_break), "");
    auto b_query = mk<Query>(std::move(b_scan));
    Loop b(std::move(b_query));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Loop* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}
TEST(Exit, CloneAndEquals) {
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // EXIT (A = ∅)
    Exit a(mk<EmptinessCheck>("A"));
    Exit b(mk<EmptinessCheck>("A"));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    Exit* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(LogRelationTimer, CloneAndEquals) {
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    /*
     * START_TIMER ON A "file.dl [8:1-8:8]"
     *   IO A()
     * END_TIMER
     * */
    std::map<std::string, std::string> a_IODir;
    std::map<std::string, std::string> b_IODir;
    LogRelationTimer a(mk<IO>("A", std::move(a_IODir)), "file.dl [8:1-8:8]", "A");
    LogRelationTimer b(mk<IO>("A", std::move(b_IODir)), "file.dl [8:1-8:8]", "A");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    LogRelationTimer* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(LogTimer, CloneAndEquals) {
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    /*
     * START_TIMER "@runtime"
     *   IO .. (..)
     * END_TIMER
     * */
    std::map<std::string, std::string> a_IODir;
    std::map<std::string, std::string> b_IODir;
    LogTimer a(mk<IO>("A", std::move(a_IODir)), "@runtime");
    LogTimer b(mk<IO>("A", std::move(a_IODir)), "@runtime");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    LogTimer* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(DebugInfo, CloneAndEquals) {
    Relation edge(
            "edge", 4, 1, {"src", "dest", "a", "b"}, {"i", "i", "i", "i"}, RelationRepresentation::DEFAULT);
    Relation path(
            "path", 4, 1, {"src", "dest", "a", "b"}, {"i", "i", "i", "i"}, RelationRepresentation::DEFAULT);
    /* BEGIN_DEBUG "path(x,y,1,(@level_num_0+1)) :- \n   edge(x,y,_,@level_num_0).\nin file /edge.dl
     * [17:1-17:26];" QUERY FOR t0 IN edge IF (NOT (edge = ∅)) IF (NOT (t0.0,t0.1,⊥,⊥) ∈ path) INSERT (t0.0,
     * t0.1, number(1), (t0.3+number(1))) INTO path END DEBUG
     * */
    VecOwn<Expression> a_insert_list;
    a_insert_list.emplace_back(new TupleElement(0, 0));
    a_insert_list.emplace_back(new TupleElement(0, 1));
    a_insert_list.emplace_back(new SignedConstant(1));
    VecOwn<Expression> a_insert_add;
    a_insert_add.emplace_back(new TupleElement(0, 3));
    a_insert_add.emplace_back(new SignedConstant(1));
    a_insert_list.emplace_back(new IntrinsicOperator(FunctorOp::ADD, std::move(a_insert_add)));
    auto a_insert = mk<Insert>("path", std::move(a_insert_list));
    VecOwn<Expression> a_filter1_list;
    a_filter1_list.emplace_back(new TupleElement(0, 0));
    a_filter1_list.emplace_back(new TupleElement(0, 1));
    a_filter1_list.emplace_back(new UndefValue);
    a_filter1_list.emplace_back(new UndefValue);
    auto a_existence_check1 = mk<ExistenceCheck>("path", std::move(a_filter1_list));
    auto a_cond1 = mk<Negation>(std::move(a_existence_check1));
    auto a_filter1 = mk<Filter>(std::move(a_cond1), std::move(a_insert), "");
    auto a_cond2 = mk<Negation>(mk<EmptinessCheck>("edge"));
    auto a_filter2 = mk<Filter>(std::move(a_cond2), std::move(a_filter1), "");
    auto a_scan = mk<Scan>("edge", 0, std::move(a_filter2), "");
    auto a_query = mk<Query>(std::move(a_scan));
    DebugInfo a(std::move(a_query),
            "path(x,y,1,(@level_num_0+1)) :- \n   edge(x,y,_,@level_num_0).\nin file /edge.dl [17:1-17:26];");

    VecOwn<Expression> b_insert_list;
    b_insert_list.emplace_back(new TupleElement(0, 0));
    b_insert_list.emplace_back(new TupleElement(0, 1));
    b_insert_list.emplace_back(new SignedConstant(1));
    VecOwn<Expression> b_insert_add;
    b_insert_add.emplace_back(new TupleElement(0, 3));
    b_insert_add.emplace_back(new SignedConstant(1));
    b_insert_list.emplace_back(new IntrinsicOperator(FunctorOp::ADD, std::move(b_insert_add)));
    auto b_insert = mk<Insert>("path", std::move(b_insert_list));
    VecOwn<Expression> b_filter1_list;
    b_filter1_list.emplace_back(new TupleElement(0, 0));
    b_filter1_list.emplace_back(new TupleElement(0, 1));
    b_filter1_list.emplace_back(new UndefValue);
    b_filter1_list.emplace_back(new UndefValue);
    auto b_existence_check1 = mk<ExistenceCheck>("path", std::move(b_filter1_list));
    auto b_cond1 = mk<Negation>(std::move(b_existence_check1));
    auto b_filter1 = mk<Filter>(std::move(b_cond1), std::move(b_insert), "");
    auto b_cond2 = mk<Negation>(mk<EmptinessCheck>("edge"));
    auto b_filter2 = mk<Filter>(std::move(b_cond2), std::move(b_filter1), "");
    auto b_scan = mk<Scan>("edge", 0, std::move(b_filter2), "");
    auto b_query = mk<Query>(std::move(b_scan));
    DebugInfo b(std::move(b_query),
            "path(x,y,1,(@level_num_0+1)) :- \n   edge(x,y,_,@level_num_0).\nin file /edge.dl [17:1-17:26];");

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    DebugInfo* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(LogSize, CloneAndEquals) {
    Relation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    LogSize a("A", "Log message");
    LogSize b("A", "Log message");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    LogSize* c = a.cloning();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}
}  // end namespace test
}  // namespace souffle::ram
