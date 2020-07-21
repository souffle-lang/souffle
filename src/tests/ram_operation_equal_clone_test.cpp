/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_operation_equal_clone_test.cpp
 *
 * Tests equal and clone function of RamCondition classes.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "AggregateOp.h"
#include "BinaryConstraintOps.h"
#include "RelationTag.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamOperation.h"
#include "ram/RamRelation.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::test {

TEST(RamScan, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // FOR t0 in A
    //  RETURN number(0)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamSignedConstant(0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    RamScan a(std::make_unique<RamRelationReference>(&A), 0, std::move(a_return), "RamScan test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamSignedConstant(0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    RamScan b(std::make_unique<RamRelationReference>(&A), 0, std::move(b_return), "RamScan test");

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamScan* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamParallelScan, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // PARALLEL FOR t0 in A
    //  RETURN number(0)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamSignedConstant(0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    RamParallelScan a(
            std::make_unique<RamRelationReference>(&A), 0, std::move(a_return), "RamParallelScan test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamSignedConstant(0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    RamParallelScan b(
            std::make_unique<RamRelationReference>(&A), 0, std::move(b_return), "RamParallelScan test");

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamParallelScan* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamIndexScan, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    RamRelation vertex("vertex", 1, 1, {"x"}, {"i"}, RelationRepresentation::DEFAULT);
    // get vertices contain self loop
    // FOR t1 IN edge ON INDEX t1.x = t1.1 AND t1.y = ⊥
    //  PROJECT (t1.0) INTO vertex
    std::vector<std::unique_ptr<RamExpression>> a_project_args;
    a_project_args.emplace_back(new RamTupleElement(1, 0));
    auto a_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&vertex), std::move(a_project_args));
    RamPattern a_criteria;
    a_criteria.first.emplace_back(new RamTupleElement(1, 1));
    a_criteria.first.emplace_back(new RamUndefValue);
    a_criteria.second.emplace_back(new RamTupleElement(1, 1));
    a_criteria.second.emplace_back(new RamUndefValue);

    RamIndexScan a(std::make_unique<RamRelationReference>(&edge), 1, std::move(a_criteria),
            std::move(a_project), "RamIndexScan test");

    std::vector<std::unique_ptr<RamExpression>> b_project_args;
    b_project_args.emplace_back(new RamTupleElement(1, 0));
    auto b_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&vertex), std::move(b_project_args));
    RamPattern b_criteria;
    b_criteria.first.emplace_back(new RamTupleElement(1, 1));
    b_criteria.first.emplace_back(new RamUndefValue);
    b_criteria.second.emplace_back(new RamTupleElement(1, 1));
    b_criteria.second.emplace_back(new RamUndefValue);

    RamIndexScan b(std::make_unique<RamRelationReference>(&edge), 1, std::move(b_criteria),
            std::move(b_project), "RamIndexScan test");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamIndexScan* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamParallelIndexScan, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    RamRelation new_edge("new_edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // get edges direct to vertex 5
    // PARALLEL FOR t1 IN edge ON INDEX t1.x = ⊥ AND t1.y = 5
    //  PROJECT (t1.0, t1.1) INTO new_edge
    std::vector<std::unique_ptr<RamExpression>> a_project_args;
    a_project_args.emplace_back(new RamTupleElement(1, 0));
    a_project_args.emplace_back(new RamTupleElement(1, 1));
    auto a_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&new_edge), std::move(a_project_args));
    RamPattern a_criteria;
    a_criteria.first.emplace_back(new RamUndefValue);
    a_criteria.first.emplace_back(new RamSignedConstant(5));
    a_criteria.second.emplace_back(new RamUndefValue);
    a_criteria.second.emplace_back(new RamSignedConstant(5));

    RamParallelIndexScan a(std::make_unique<RamRelationReference>(&edge), 1, std::move(a_criteria),
            std::move(a_project), "RamParallelIndexScan test");

    std::vector<std::unique_ptr<RamExpression>> b_project_args;
    b_project_args.emplace_back(new RamTupleElement(1, 0));
    b_project_args.emplace_back(new RamTupleElement(1, 1));
    auto b_project = std::make_unique<RamProject>(
            std::make_unique<RamRelationReference>(&new_edge), std::move(b_project_args));
    RamPattern b_criteria;
    b_criteria.first.emplace_back(new RamUndefValue);
    b_criteria.first.emplace_back(new RamSignedConstant(5));
    b_criteria.second.emplace_back(new RamUndefValue);
    b_criteria.second.emplace_back(new RamSignedConstant(5));

    RamParallelIndexScan b(std::make_unique<RamRelationReference>(&edge), 1, std::move(b_criteria),
            std::move(b_project), "RamParallelIndexScan test");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamParallelIndexScan* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamChoice, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // choose an edge not adjcent to vertex 5
    // CHOICE t1 IN edge WHERE NOT t1.0 = 5 AND NOT t1.1 = 5
    //  RETURN (t1.0, t1.1)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamTupleElement(1, 0));
    a_return_args.emplace_back(new RamTupleElement(1, 1));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    auto a_constraint1 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 0), std::make_unique<RamSignedConstant>(5));
    auto a_neg1 = std::make_unique<RamNegation>(std::move(a_constraint1));
    auto a_constraint2 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto a_neg2 = std::make_unique<RamNegation>(std::move(a_constraint2));
    auto a_cond = std::make_unique<RamConjunction>(std::move(a_neg1), std::move(a_neg2));
    RamChoice a(std::make_unique<RamRelationReference>(&edge), 1, std::move(a_cond), std::move(a_return),
            "RamChoice test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamTupleElement(1, 0));
    b_return_args.emplace_back(new RamTupleElement(1, 1));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    auto b_constraint1 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 0), std::make_unique<RamSignedConstant>(5));
    auto b_neg1 = std::make_unique<RamNegation>(std::move(b_constraint1));
    auto b_constraint2 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto b_neg2 = std::make_unique<RamNegation>(std::move(b_constraint2));
    auto b_cond = std::make_unique<RamConjunction>(std::move(b_neg1), std::move(b_neg2));
    RamChoice b(std::make_unique<RamRelationReference>(&edge), 1, std::move(b_cond), std::move(b_return),
            "RamChoice test");

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamChoice* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamParallelChoice, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // parallel choose an edge not adjcent to vertex 5
    // PARALLEL CHOICE t1 IN edge WHERE NOT t1.0 = 5 AND NOT t1.1 = 5
    //  RETURN (t1.0, t1.1)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamTupleElement(1, 0));
    a_return_args.emplace_back(new RamTupleElement(1, 1));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    auto a_constraint1 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 0), std::make_unique<RamSignedConstant>(5));
    auto a_neg1 = std::make_unique<RamNegation>(std::move(a_constraint1));
    auto a_constraint2 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto a_neg2 = std::make_unique<RamNegation>(std::move(a_constraint2));
    auto a_cond = std::make_unique<RamConjunction>(std::move(a_neg1), std::move(a_neg2));
    RamParallelChoice a(std::make_unique<RamRelationReference>(&edge), 1, std::move(a_cond),
            std::move(a_return), "RamParallelChoice test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamTupleElement(1, 0));
    b_return_args.emplace_back(new RamTupleElement(1, 1));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    auto b_constraint1 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 0), std::make_unique<RamSignedConstant>(5));
    auto b_neg1 = std::make_unique<RamNegation>(std::move(b_constraint1));
    auto b_constraint2 = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto b_neg2 = std::make_unique<RamNegation>(std::move(b_constraint2));
    auto b_cond = std::make_unique<RamConjunction>(std::move(b_neg1), std::move(b_neg2));
    RamParallelChoice b(std::make_unique<RamRelationReference>(&edge), 1, std::move(b_cond),
            std::move(b_return), "RamParallelChoice test");

    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamParallelChoice* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamIndexChoice, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // FOR t1 IN edge ON INDEX t1.x = 5 AND t1.y = ⊥
    // WHERE NOT t1.1 = 5
    //  RETURN (t1.0, t1.1)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamTupleElement(1, 0));
    a_return_args.emplace_back(new RamTupleElement(1, 1));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    auto a_constraint = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto a_neg = std::make_unique<RamNegation>(std::move(a_constraint));
    RamPattern a_criteria;
    a_criteria.first.emplace_back(new RamSignedConstant(5));
    a_criteria.first.emplace_back(new RamUndefValue);
    a_criteria.second.emplace_back(new RamSignedConstant(5));
    a_criteria.second.emplace_back(new RamUndefValue);
    RamIndexChoice a(std::make_unique<RamRelationReference>(&edge), 1, std::move(a_neg),
            std::move(a_criteria), std::move(a_return), "RamIndexChoice test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamTupleElement(1, 0));
    b_return_args.emplace_back(new RamTupleElement(1, 1));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    auto b_constraint = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto b_neg = std::make_unique<RamNegation>(std::move(b_constraint));
    RamPattern b_criteria;
    b_criteria.first.emplace_back(new RamSignedConstant(5));
    b_criteria.first.emplace_back(new RamUndefValue);
    b_criteria.second.emplace_back(new RamSignedConstant(5));
    b_criteria.second.emplace_back(new RamUndefValue);
    RamIndexChoice b(std::make_unique<RamRelationReference>(&edge), 1, std::move(b_neg),
            std::move(b_criteria), std::move(b_return), "RamIndexChoice test");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamIndexChoice* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamiParallelIndexChoice, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // PARALLEL FOR t1 IN edge ON INDEX t1.x = 5 AND t1.y = ⊥
    // WHERE NOT t1.1 = 5
    //  RETURN (t1.0, t1.1)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamTupleElement(1, 0));
    a_return_args.emplace_back(new RamTupleElement(1, 1));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    auto a_constraint = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto a_neg = std::make_unique<RamNegation>(std::move(a_constraint));
    RamPattern a_criteria;
    a_criteria.first.emplace_back(new RamSignedConstant(5));
    a_criteria.first.emplace_back(new RamUndefValue);
    a_criteria.second.emplace_back(new RamSignedConstant(5));
    a_criteria.second.emplace_back(new RamUndefValue);
    RamParallelIndexChoice a(std::make_unique<RamRelationReference>(&edge), 1, std::move(a_neg),
            std::move(a_criteria), std::move(a_return), "RamIndexChoice test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamTupleElement(1, 0));
    b_return_args.emplace_back(new RamTupleElement(1, 1));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    auto b_constraint = std::make_unique<RamConstraint>(BinaryConstraintOp::EQ,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(5));
    auto b_neg = std::make_unique<RamNegation>(std::move(b_constraint));
    RamPattern b_criteria;
    b_criteria.first.emplace_back(new RamSignedConstant(5));
    b_criteria.first.emplace_back(new RamUndefValue);
    b_criteria.second.emplace_back(new RamSignedConstant(5));
    b_criteria.second.emplace_back(new RamUndefValue);
    RamParallelIndexChoice b(std::make_unique<RamRelationReference>(&edge), 1, std::move(b_neg),
            std::move(b_criteria), std::move(b_return), "RamIndexChoice test");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamParallelIndexChoice* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamAggregate, CloneAndEquals) {
    RamRelation edge("edge", 2, 1, {"x", "y"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // t0.0 = COUNT FOR ALL t1 IN edge
    //  RETURN t0.0
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamTupleElement(0, 0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    RamAggregate a(std::move(a_return), AggregateOp::COUNT, std::make_unique<RamRelationReference>(&edge),
            std::make_unique<RamTupleElement>(0, 0), std::make_unique<RamTrue>(), 1);

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamTupleElement(0, 0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    RamAggregate b(std::move(b_return), AggregateOp::COUNT, std::make_unique<RamRelationReference>(&edge),
            std::make_unique<RamTupleElement>(0, 0), std::make_unique<RamTrue>(), 1);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamAggregate* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamIndexAggregate, CloneAndEquals) {
    RamRelation sqrt("sqrt", 2, 1, {"nth", "value"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // t0.0 = MIN t1.1 SEARCH t1 IN sqrt ON INDEX t1.0 = ⊥ AND t1.1 = ⊥
    // WHERE t1.1 > 80
    //  RETURN t0.0
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamTupleElement(0, 0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    auto a_cond = std::make_unique<RamConstraint>(BinaryConstraintOp::GE,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(80));
    RamPattern a_criteria;
    a_criteria.first.emplace_back(new RamUndefValue);
    a_criteria.first.emplace_back(new RamUndefValue);
    a_criteria.second.emplace_back(new RamUndefValue);
    a_criteria.second.emplace_back(new RamUndefValue);
    RamIndexAggregate a(std::move(a_return), AggregateOp::MIN, std::make_unique<RamRelationReference>(&sqrt),
            std::make_unique<RamTupleElement>(1, 1), std::move(a_cond), std::move(a_criteria), 1);

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamTupleElement(0, 0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    auto b_cond = std::make_unique<RamConstraint>(BinaryConstraintOp::GE,
            std::make_unique<RamTupleElement>(1, 1), std::make_unique<RamSignedConstant>(80));
    RamPattern b_criteria;
    b_criteria.first.emplace_back(new RamUndefValue);
    b_criteria.first.emplace_back(new RamUndefValue);
    b_criteria.second.emplace_back(new RamUndefValue);
    b_criteria.second.emplace_back(new RamUndefValue);
    RamIndexAggregate b(std::move(b_return), AggregateOp::MIN, std::make_unique<RamRelationReference>(&sqrt),
            std::make_unique<RamTupleElement>(1, 1), std::move(b_cond), std::move(b_criteria), 1);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamIndexAggregate* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamUnpackedRecord, CloneAndEquals) {
    // UNPACK (t0.0, t0.2) INTO t1
    // RETURN number(0)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamSignedConstant(0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    std::vector<std::unique_ptr<RamExpression>> a_record_args;
    a_record_args.emplace_back(new RamTupleElement(0, 0));
    a_record_args.emplace_back(new RamTupleElement(0, 2));
    auto a_record = std::make_unique<RamPackRecord>(std::move(a_record_args));
    RamUnpackRecord a(std::move(a_return), 1, std::move(a_record), 2);

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamSignedConstant(0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    std::vector<std::unique_ptr<RamExpression>> b_record_args;
    b_record_args.emplace_back(new RamTupleElement(0, 0));
    b_record_args.emplace_back(new RamTupleElement(0, 2));
    auto b_record = std::make_unique<RamPackRecord>(std::move(b_record_args));
    RamUnpackRecord b(std::move(b_return), 1, std::move(b_record), 2);
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamUnpackRecord* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamFilter, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"a"}, {"i"}, RelationRepresentation::DEFAULT);
    // IF (NOT t0.1 in A)
    // RETURN number(0)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamSignedConstant(0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    std::vector<std::unique_ptr<RamExpression>> a_existence_check_args;
    a_existence_check_args.emplace_back(new RamTupleElement(0, 1));
    auto a_existence_check = std::make_unique<RamExistenceCheck>(
            std::make_unique<RamRelationReference>(&A), std::move(a_existence_check_args));
    RamFilter a(std::make_unique<RamNegation>(std::move(a_existence_check)), std::move(a_return),
            "RamFilter test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamSignedConstant(0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    std::vector<std::unique_ptr<RamExpression>> b_existence_check_args;
    b_existence_check_args.emplace_back(new RamTupleElement(0, 1));
    auto b_existence_check = std::make_unique<RamExistenceCheck>(
            std::make_unique<RamRelationReference>(&A), std::move(b_existence_check_args));
    RamFilter b(std::make_unique<RamNegation>(std::move(b_existence_check)), std::move(b_return),
            "RamFilter test");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamFilter* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamBreak, CloneAndEquals) {
    RamRelation A("A", 1, 1, {"a"}, {"i"}, RelationRepresentation::DEFAULT);
    // IF (A = ∅) BREAK
    // RETURN number(0)
    std::vector<std::unique_ptr<RamExpression>> a_return_args;
    a_return_args.emplace_back(new RamSignedConstant(0));
    auto a_return = std::make_unique<RamSubroutineReturn>(std::move(a_return_args));
    RamBreak a(std::make_unique<RamEmptinessCheck>(std::make_unique<RamRelationReference>(&A)),
            std::move(a_return), "RamBreak test");

    std::vector<std::unique_ptr<RamExpression>> b_return_args;
    b_return_args.emplace_back(new RamSignedConstant(0));
    auto b_return = std::make_unique<RamSubroutineReturn>(std::move(b_return_args));
    RamBreak b(std::make_unique<RamEmptinessCheck>(std::make_unique<RamRelationReference>(&A)),
            std::move(b_return), "RamBreak test");
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamBreak* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamProject, CloneAndEquals) {
    RamRelation A("A", 2, 1, {"a", "b"}, {"i", "i"}, RelationRepresentation::DEFAULT);
    // PROJECT (t0.1, t0.3) INTO A
    std::vector<std::unique_ptr<RamExpression>> a_args;
    a_args.emplace_back(new RamTupleElement(0, 1));
    a_args.emplace_back(new RamTupleElement(0, 3));
    RamProject a(std::make_unique<RamRelationReference>(&A), std::move(a_args));

    std::vector<std::unique_ptr<RamExpression>> b_args;
    b_args.emplace_back(new RamTupleElement(0, 1));
    b_args.emplace_back(new RamTupleElement(0, 3));
    RamProject b(std::make_unique<RamRelationReference>(&A), std::move(b_args));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamProject* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;
}

TEST(RamSubroutineReturn, CloneAndEquals) {
    // RETURN (t0.1, t0.2)
    std::vector<std::unique_ptr<RamExpression>> a_args;
    a_args.emplace_back(new RamTupleElement(0, 1));
    a_args.emplace_back(new RamTupleElement(0, 2));
    RamSubroutineReturn a(std::move(a_args));

    std::vector<std::unique_ptr<RamExpression>> b_args;
    b_args.emplace_back(new RamTupleElement(0, 1));
    b_args.emplace_back(new RamTupleElement(0, 2));
    RamSubroutineReturn b(std::move(b_args));
    EXPECT_EQ(a, b);
    EXPECT_NE(&a, &b);

    RamSubroutineReturn* c = a.clone();
    EXPECT_EQ(a, *c);
    EXPECT_NE(&a, c);
    delete c;

    // RETURN (number(0))
    std::vector<std::unique_ptr<RamExpression>> d_args;
    d_args.emplace_back(new RamSignedConstant(0));
    RamSubroutineReturn d(std::move(d_args));

    std::vector<std::unique_ptr<RamExpression>> e_args;
    e_args.emplace_back(new RamSignedConstant(0));
    RamSubroutineReturn e(std::move(e_args));
    EXPECT_EQ(d, e);
    EXPECT_NE(&d, &e);

    RamSubroutineReturn* f = d.clone();
    EXPECT_EQ(d, *f);
    EXPECT_NE(&d, f);
    delete f;
}
}  // end namespace souffle::test
