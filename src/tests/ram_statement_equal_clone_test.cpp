/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_statement_equal_clone_test.cpp
 *
 * Tests equal and clone function of RamStatement classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "RelationTag.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamOperation.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

namespace test {

TEST(RamIO1, CloneAndEquals) {
    // IO A ()
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    std::map<std::string, std::string> ioEmptyA;
    std::map<std::string, std::string> ioEmptyB;
    RamIO a(std::make_unique<RamRelationReference>(&A), std::move(ioEmptyA));
    RamIO b(std::make_unique<RamRelationReference>(&A), std::move(ioEmptyB));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamIO* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamClear, CloneAndEquals) {
    // CLEAR A
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamClear a(std::make_unique<RamRelationReference>(&A));
    RamClear b(std::make_unique<RamRelationReference>(&A));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamClear* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamExtend, CloneAndEquals) {
    // MERGE B WITH A
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamRelation B("B", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamExtend a(std::make_unique<RamRelationReference>(&B), std::make_unique<RamRelationReference>(&A));
    RamExtend b(std::make_unique<RamRelationReference>(&B), std::make_unique<RamRelationReference>(&A));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamExtend* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamSwap, CloneAndEquals) {
    // SWAP(A,B)
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamRelation B("B", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamSwap a(std::make_unique<RamRelationReference>(&A), std::make_unique<RamRelationReference>(&B));
    RamSwap b(std::make_unique<RamRelationReference>(&A), std::make_unique<RamRelationReference>(&B));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamSwap* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamQuery, CloneAndEquals) {
    RamRelation A("A", 3, 1, {"a", "b", "c"}, {"i", "s", "i"}, RelationRepresentation::DEFAULT);
    RamRelation B("B", 2, 1, {"a", "c"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    /*
     * QUERY
     *  FOR t0 IN A
     *   PROJECT (t0.0, t0.2) INTO B
     */
    std::vector<std::unique_ptr<RamExpression>> a_expressions;
    a_expressions.emplace_back(new RamTupleElement(0, 0));
    a_expressions.emplace_back(new RamTupleElement(0, 2));
    auto a_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&B), std::move(a_expressions));
    auto a_scan = std::make_unique<RamScan>(
            std::make_unique<RamRelationReference>(&A), 0, std::move(a_project), "");

    std::vector<std::unique_ptr<RamExpression>> b_expressions;
    b_expressions.emplace_back(new RamTupleElement(0, 0));
    b_expressions.emplace_back(new RamTupleElement(0, 2));
    auto b_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&B), std::move(b_expressions));
    auto b_scan = std::make_unique<RamScan>(
            std::make_unique<RamRelationReference>(&A), 0, std::move(b_project), "");

    RamQuery a(std::move(a_scan));
    RamQuery b(std::move(b_scan));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamQuery* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;

    /*
     * QUERY
     *  PARALLEL CHOICE t1 IN A WHERE (t1.0 = 0)
     *   RETURN (t1.2)
     */
    std::vector<std::unique_ptr<RamExpression>> d_return_value;
    d_return_value.emplace_back(new RamTupleElement(1, 0));
    auto d_return = std::make_unique<RamSubroutineReturn>(std::move(d_return_value));
    // condition t1.0 = 0
    auto d_cond = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 0), std::make_unique<RamSignedConstant>(0));
    auto d_parallel_choice = std::make_unique<RamParallelChoice>(
            std::make_unique<RamRelationReference>(&A), 1, std::move(d_cond), std::move(d_return), "");

    std::vector<std::unique_ptr<RamExpression>> e_return_value;
    e_return_value.emplace_back(new RamTupleElement(1, 0));
    auto e_return = std::make_unique<RamSubroutineReturn>(std::move(e_return_value));
    // condition t1.0 = 0
    auto e_cond = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 0), std::make_unique<RamSignedConstant>(0));
    auto e_parallel_choice = std::make_unique<RamParallelChoice>(
            std::make_unique<RamRelationReference>(&A), 1, std::move(e_cond), std::move(e_return), "");
    RamQuery d(std::move(d_parallel_choice));
    RamQuery e(std::move(e_parallel_choice));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamQuery* f = d.clone();
    EXPECT_EQ(d, *f);
    EXPECT_NE(&d, f);
    delete f;
}

TEST(RamSequence, CloneAndEquals) {
    // no statements in the sequence
    RamSequence a;
    RamSequence b;
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamSequence* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;

    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // one statement in the sequence
    // CLEAR A
    RamSequence d(std::make_unique<RamClear>(std::make_unique<RamRelationReference>(&A)));
    RamSequence e(std::make_unique<RamClear>(std::make_unique<RamRelationReference>(&A)));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamSequence* f = d.clone();
    EXPECT_EQ(d, *f);
    EXPECT_NE(&d, f);
    delete f;

    // multiple statements in the sequence
    // IO A ()
    // CLEAR A
    std::map<std::string, std::string> g_load_IODir;
    std::map<std::string, std::string> h_load_IODir;
    RamSequence g(
            std::make_unique<RamIO>(std::make_unique<RamRelationReference>(&A), std::move(g_load_IODir)),
            std::make_unique<RamClear>(std::make_unique<RamRelationReference>(&A)));
    RamSequence h(
            std::make_unique<RamIO>(std::make_unique<RamRelationReference>(&A), std::move(h_load_IODir)),
            std::make_unique<RamClear>(std::make_unique<RamRelationReference>(&A)));
    EXPECT_EQ(g, h);
    EXPECT_NE(&g, &h);

    RamSequence* i = g.clone();
    EXPECT_EQ(g, *i);
    EXPECT_NE(&g, i);
    delete i;
}

TEST(RamParallel, CloneAndEquals) {
    RamRelation A("A", 3, 1, {"a", "b", "c"}, {"i", "s", "i"}, RelationRepresentation::DEFAULT);
    RamRelation B("B", 2, 1, {"a", "c"}, {"i", "i"}, RelationRepresentation::DEFAULT);

    /* PARALLEL
     *  QUERY
     *   FOR t0 IN A
     *    IF (t0.0 > 0)
     *     PROJECT (t0.0, t0.2) INTO B
     * END PARALLEL
     * */

    std::vector<std::unique_ptr<RamExpression>> a_expressions;
    a_expressions.emplace_back(new RamTupleElement(0, 0));
    a_expressions.emplace_back(new RamTupleElement(0, 2));
    auto a_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&B), std::move(a_expressions));
    auto a_cond = std::make_unique<RamFilter>(
            std::make_unique<RamConstraint>(BinaryConstraintOp::GE, std::make_unique<RamTupleElement>(0, 0),
                    std::make_unique<RamSignedConstant>(0)),
            std::move(a_project), "");
    auto a_scan =
            std::make_unique<RamScan>(std::make_unique<RamRelationReference>(&A), 0, std::move(a_cond), "");
    auto a_query = std::make_unique<RamQuery>(std::move(a_scan));
    RamParallel a(std::move(a_query));

    std::vector<std::unique_ptr<RamExpression>> b_expressions;
    b_expressions.emplace_back(new RamTupleElement(0, 0));
    b_expressions.emplace_back(new RamTupleElement(0, 2));
    auto b_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&B), std::move(b_expressions));
    auto b_cond = std::make_unique<RamFilter>(
            std::make_unique<RamConstraint>(BinaryConstraintOp::GE, std::make_unique<RamTupleElement>(0, 0),
                    std::make_unique<RamSignedConstant>(0)),
            std::move(b_project), "");
    auto b_scan =
            std::make_unique<RamScan>(std::make_unique<RamRelationReference>(&A), 0, std::move(b_cond), "");
    auto b_query = std::make_unique<RamQuery>(std::move(b_scan));
    RamParallel b(std::move(b_query));

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamParallel* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}
TEST(RamLoop, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamRelation B("B", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    /*
     * LOOP
     *  QUERY
     *   FOR t0 IN A
     *    IF t0.0 = 4 BREAK
     *    PROJECT (t0.0) INTO B
     * END LOOP
     * */
    std::vector<std::unique_ptr<RamExpression>> a_expressions;
    a_expressions.emplace_back(new RamTupleElement(0, 0));
    auto a_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&B), std::move(a_expressions));
    auto a_break = std::make_unique<RamBreak>(
            std::make_unique<RamConstraint>(BinaryConstraintOp::EQ, std::make_unique<RamTupleElement>(0, 0),
                    std::make_unique<RamSignedConstant>(4)),
            std::move(a_project), "");
    auto a_scan =
            std::make_unique<RamScan>(std::make_unique<RamRelationReference>(&A), 0, std::move(a_break), "");
    auto a_query = std::make_unique<RamQuery>(std::move(a_scan));
    RamLoop a(std::move(a_query));

    std::vector<std::unique_ptr<RamExpression>> b_expressions;
    b_expressions.emplace_back(new RamTupleElement(0, 0));
    auto b_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&B), std::move(b_expressions));
    auto b_break = std::make_unique<RamBreak>(
            std::make_unique<RamConstraint>(BinaryConstraintOp::EQ, std::make_unique<RamTupleElement>(0, 0),
                    std::make_unique<RamSignedConstant>(4)),
            std::move(b_project), "");
    auto b_scan =
            std::make_unique<RamScan>(std::make_unique<RamRelationReference>(&A), 0, std::move(b_break), "");
    auto b_query = std::make_unique<RamQuery>(std::move(b_scan));
    RamLoop b(std::move(b_query));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamLoop* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}
TEST(RamExit, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // EXIT (A = ∅)
    RamExit a(std::make_unique<RamEmptinessCheck>(std::make_unique<RamRelationReference>(&A)));
    RamExit b(std::make_unique<RamEmptinessCheck>(std::make_unique<RamRelationReference>(&A)));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamExit* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamLogRelationTimer, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    /*
     * START_TIMER ON A "file.dl [8:1-8:8]"
     *   IO A()
     * END_TIMER
     * */
    std::map<std::string, std::string> a_IODir;
    std::map<std::string, std::string> b_IODir;
    RamLogRelationTimer a(
            std::make_unique<RamIO>(std::make_unique<RamRelationReference>(&A), std::move(a_IODir)),
            "file.dl [8:1-8:8]", std::make_unique<RamRelationReference>(&A));
    RamLogRelationTimer b(
            std::make_unique<RamIO>(std::make_unique<RamRelationReference>(&A), std::move(b_IODir)),
            "file.dl [8:1-8:8]", std::make_unique<RamRelationReference>(&A));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamLogRelationTimer* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamLogTimer, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    /*
     * START_TIMER "@runtime"
     *   IO .. (..)
     * END_TIMER
     * */
    std::map<std::string, std::string> a_IODir;
    std::map<std::string, std::string> b_IODir;
    RamLogTimer a(std::make_unique<RamIO>(std::make_unique<RamRelationReference>(&A), std::move(a_IODir)),
            "@runtime");
    RamLogTimer b(std::make_unique<RamIO>(std::make_unique<RamRelationReference>(&A), std::move(a_IODir)),
            "@runtime");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamLogTimer* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamDebugInfo, CloneAndEquals) {
    RamRelation edge(
            "edge", 4, 1, {"src", "dest", "a", "b"}, {"i", "i", "i", "i"}, RelationRepresentation::DEFAULT);
    RamRelation path(
            "path", 4, 1, {"src", "dest", "a", "b"}, {"i", "i", "i", "i"}, RelationRepresentation::DEFAULT);
    /* BEGIN_DEBUG "path(x,y,1,(@level_num_0+1)) :- \n   edge(x,y,_,@level_num_0).\nin file /edge.dl
     * [17:1-17:26];" QUERY FOR t0 IN edge IF (NOT (edge = ∅)) IF (NOT (t0.0,t0.1,⊥,⊥) ∈ path) PROJECT (t0.0,
     * t0.1, number(1), (t0.3+number(1))) INTO path END DEBUG
     * */
    std::vector<std::unique_ptr<RamExpression>> a_project_list;
    a_project_list.emplace_back(new RamTupleElement(0, 0));
    a_project_list.emplace_back(new RamTupleElement(0, 1));
    a_project_list.emplace_back(new RamSignedConstant(1));
    std::vector<std::unique_ptr<RamExpression>> a_project_add;
    a_project_add.emplace_back(new RamTupleElement(0, 3));
    a_project_add.emplace_back(new RamSignedConstant(1));
    a_project_list.emplace_back(new RamIntrinsicOperator(FunctorOp::ADD, std::move(a_project_add)));
    auto a_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&path), std::move(a_project_list));
    std::vector<std::unique_ptr<RamExpression>> a_filter1_list;
    a_filter1_list.emplace_back(new RamTupleElement(0, 0));
    a_filter1_list.emplace_back(new RamTupleElement(0, 1));
    a_filter1_list.emplace_back(new RamUndefValue);
    a_filter1_list.emplace_back(new RamUndefValue);
    auto a_existence_check1 = std::make_unique<RamExistenceCheck>(
            std::make_unique<RamRelationReference>(&path), std::move(a_filter1_list));
    auto a_cond1 = std::make_unique<RamNegation>(std::move(a_existence_check1));
    auto a_filter1 = std::make_unique<RamFilter>(std::move(a_cond1), std::move(a_project), "");
    auto a_cond2 = std::make_unique<RamNegation>(
            std::make_unique<RamEmptinessCheck>(std::make_unique<RamRelationReference>(&edge)));
    auto a_filter2 = std::make_unique<RamFilter>(std::move(a_cond2), std::move(a_filter1), "");
    auto a_scan = std::make_unique<RamScan>(
            std::make_unique<RamRelationReference>(&edge), 0, std::move(a_filter2), "");
    auto a_query = std::make_unique<RamQuery>(std::move(a_scan));
    RamDebugInfo a(std::move(a_query),
            "path(x,y,1,(@level_num_0+1)) :- \n   edge(x,y,_,@level_num_0).\nin file /edge.dl [17:1-17:26];");

    std::vector<std::unique_ptr<RamExpression>> b_project_list;
    b_project_list.emplace_back(new RamTupleElement(0, 0));
    b_project_list.emplace_back(new RamTupleElement(0, 1));
    b_project_list.emplace_back(new RamSignedConstant(1));
    std::vector<std::unique_ptr<RamExpression>> b_project_add;
    b_project_add.emplace_back(new RamTupleElement(0, 3));
    b_project_add.emplace_back(new RamSignedConstant(1));
    b_project_list.emplace_back(new RamIntrinsicOperator(FunctorOp::ADD, std::move(b_project_add)));
    auto b_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&path), std::move(b_project_list));
    std::vector<std::unique_ptr<RamExpression>> b_filter1_list;
    b_filter1_list.emplace_back(new RamTupleElement(0, 0));
    b_filter1_list.emplace_back(new RamTupleElement(0, 1));
    b_filter1_list.emplace_back(new RamUndefValue);
    b_filter1_list.emplace_back(new RamUndefValue);
    auto b_existence_check1 = std::make_unique<RamExistenceCheck>(
            std::make_unique<RamRelationReference>(&path), std::move(b_filter1_list));
    auto b_cond1 = std::make_unique<RamNegation>(std::move(b_existence_check1));
    auto b_filter1 = std::make_unique<RamFilter>(std::move(b_cond1), std::move(b_project), "");
    auto b_cond2 = std::make_unique<RamNegation>(
            std::make_unique<RamEmptinessCheck>(std::make_unique<RamRelationReference>(&edge)));
    auto b_filter2 = std::make_unique<RamFilter>(std::move(b_cond2), std::move(b_filter1), "");
    auto b_scan = std::make_unique<RamScan>(
            std::make_unique<RamRelationReference>(&edge), 0, std::move(b_filter2), "");
    auto b_query = std::make_unique<RamQuery>(std::move(b_scan));
    RamDebugInfo b(std::move(b_query),
            "path(x,y,1,(@level_num_0+1)) :- \n   edge(x,y,_,@level_num_0).\nin file /edge.dl [17:1-17:26];");

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamDebugInfo* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamLogSize, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    RamLogSize a(std::make_unique<RamRelationReference>(&A), "Log message");
    RamLogSize b(std::make_unique<RamRelationReference>(&A), "Log message");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamLogSize* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}
}  // end namespace test
}  // end namespace souffle
