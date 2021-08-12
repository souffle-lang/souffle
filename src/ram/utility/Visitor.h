/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Visitor.h
 *
 * Provides some infrastructure for the implementation of operations
 * on top of RAM structures.
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractConditional.h"
#include "ram/AbstractExistenceCheck.h"
#include "ram/AbstractOperator.h"
#include "ram/Aggregate.h"
#include "ram/AutoIncrement.h"
#include "ram/BinRelationStatement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/Extend.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/FloatConstant.h"
#include "ram/GuardedInsert.h"
#include "ram/IO.h"
#include "ram/IfExists.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexIfExists.h"
#include "ram/IndexOperation.h"
#include "ram/IndexScan.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/ListStatement.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/NumericConstant.h"
#include "ram/Operation.h"
#include "ram/PackRecord.h"
#include "ram/Parallel.h"
#include "ram/ParallelAggregate.h"
#include "ram/ParallelIfExists.h"
#include "ram/ParallelIndexAggregate.h"
#include "ram/ParallelIndexIfExists.h"
#include "ram/ParallelIndexScan.h"
#include "ram/ParallelScan.h"
#include "ram/Program.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "ram/RelationSize.h"
#include "ram/RelationStatement.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/StringConstant.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/Swap.h"
#include "ram/True.h"
#include "ram/TupleElement.h"
#include "ram/TupleOperation.h"
#include "ram/UndefValue.h"
#include "ram/UnpackRecord.h"
#include "ram/UnsignedConstant.h"
#include "ram/UserDefinedOperator.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/Visitor.h"
#include <cstddef>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace souffle::ram {
/**
 * The generic base type of all RamVisitors
 * @see souffle::Visitor
 */
template <typename R = void, typename NodeType = Node const, typename... Params>
struct Visitor : souffle::detail::VisitorBase<R, NodeType, Params...> {
    using Base = souffle::detail::VisitorBase<R, NodeType, Params...>;
    using Base::visit_;

    R dispatch(NodeType& node, Params... args) override {
        // dispatch node processing based on dynamic type

        // Relation
        SOUFFLE_VISITOR_FORWARD(Relation);

        // Expressions
        SOUFFLE_VISITOR_FORWARD(TupleElement);
        SOUFFLE_VISITOR_FORWARD(SignedConstant);
        SOUFFLE_VISITOR_FORWARD(UnsignedConstant);
        SOUFFLE_VISITOR_FORWARD(FloatConstant);
        SOUFFLE_VISITOR_FORWARD(NumericConstant);
        SOUFFLE_VISITOR_FORWARD(StringConstant);
        SOUFFLE_VISITOR_FORWARD(IntrinsicOperator);
        SOUFFLE_VISITOR_FORWARD(UserDefinedOperator);
        SOUFFLE_VISITOR_FORWARD(AutoIncrement);
        SOUFFLE_VISITOR_FORWARD(PackRecord);
        SOUFFLE_VISITOR_FORWARD(SubroutineArgument);
        SOUFFLE_VISITOR_FORWARD(UndefValue);
        SOUFFLE_VISITOR_FORWARD(RelationSize);

        // Conditions
        SOUFFLE_VISITOR_FORWARD(True);
        SOUFFLE_VISITOR_FORWARD(False);
        SOUFFLE_VISITOR_FORWARD(EmptinessCheck);
        SOUFFLE_VISITOR_FORWARD(ProvenanceExistenceCheck);
        SOUFFLE_VISITOR_FORWARD(ExistenceCheck);
        SOUFFLE_VISITOR_FORWARD(Conjunction);
        SOUFFLE_VISITOR_FORWARD(Negation);
        SOUFFLE_VISITOR_FORWARD(Constraint);

        // Operations
        SOUFFLE_VISITOR_FORWARD(Filter);
        SOUFFLE_VISITOR_FORWARD(Break);
        SOUFFLE_VISITOR_FORWARD(GuardedInsert);
        SOUFFLE_VISITOR_FORWARD(Insert);
        SOUFFLE_VISITOR_FORWARD(SubroutineReturn);
        SOUFFLE_VISITOR_FORWARD(UnpackRecord);
        SOUFFLE_VISITOR_FORWARD(NestedIntrinsicOperator);
        SOUFFLE_VISITOR_FORWARD(ParallelScan);
        SOUFFLE_VISITOR_FORWARD(Scan);
        SOUFFLE_VISITOR_FORWARD(ParallelIndexScan);
        SOUFFLE_VISITOR_FORWARD(IndexScan);
        SOUFFLE_VISITOR_FORWARD(ParallelIfExists);
        SOUFFLE_VISITOR_FORWARD(IfExists);
        SOUFFLE_VISITOR_FORWARD(ParallelIndexIfExists);
        SOUFFLE_VISITOR_FORWARD(IndexIfExists);
        SOUFFLE_VISITOR_FORWARD(ParallelAggregate);
        SOUFFLE_VISITOR_FORWARD(Aggregate);
        SOUFFLE_VISITOR_FORWARD(ParallelIndexAggregate);
        SOUFFLE_VISITOR_FORWARD(IndexAggregate);

        // Statements
        SOUFFLE_VISITOR_FORWARD(IO);
        SOUFFLE_VISITOR_FORWARD(Query);
        SOUFFLE_VISITOR_FORWARD(Clear);
        SOUFFLE_VISITOR_FORWARD(LogSize);

        SOUFFLE_VISITOR_FORWARD(Swap);
        SOUFFLE_VISITOR_FORWARD(Extend);

        // Control-flow
        SOUFFLE_VISITOR_FORWARD(Program);
        SOUFFLE_VISITOR_FORWARD(Sequence);
        SOUFFLE_VISITOR_FORWARD(Loop);
        SOUFFLE_VISITOR_FORWARD(Parallel);
        SOUFFLE_VISITOR_FORWARD(Exit);
        SOUFFLE_VISITOR_FORWARD(LogTimer);
        SOUFFLE_VISITOR_FORWARD(LogRelationTimer);
        SOUFFLE_VISITOR_FORWARD(DebugInfo);
        SOUFFLE_VISITOR_FORWARD(Call);

        // did not work ...
        fatal("unsupported type: %s", typeid(node).name());
    }

protected:
    // -- statements --
    SOUFFLE_VISITOR_LINK(IO, RelationStatement);
    SOUFFLE_VISITOR_LINK(Query, Statement);
    SOUFFLE_VISITOR_LINK(Clear, RelationStatement);
    SOUFFLE_VISITOR_LINK(LogSize, RelationStatement);

    SOUFFLE_VISITOR_LINK(RelationStatement, Statement);

    SOUFFLE_VISITOR_LINK(Swap, BinRelationStatement);
    SOUFFLE_VISITOR_LINK(Extend, BinRelationStatement);
    SOUFFLE_VISITOR_LINK(BinRelationStatement, Statement);

    SOUFFLE_VISITOR_LINK(Sequence, ListStatement);
    SOUFFLE_VISITOR_LINK(Loop, Statement);
    SOUFFLE_VISITOR_LINK(Parallel, ListStatement);
    SOUFFLE_VISITOR_LINK(ListStatement, Statement);
    SOUFFLE_VISITOR_LINK(Exit, Statement);
    SOUFFLE_VISITOR_LINK(LogTimer, Statement);
    SOUFFLE_VISITOR_LINK(LogRelationTimer, Statement);
    SOUFFLE_VISITOR_LINK(DebugInfo, Statement);
    SOUFFLE_VISITOR_LINK(Call, Statement);

    SOUFFLE_VISITOR_LINK(Statement, Node);

    // -- operations --
    SOUFFLE_VISITOR_LINK(GuardedInsert, Insert);
    SOUFFLE_VISITOR_LINK(Insert, Operation);
    SOUFFLE_VISITOR_LINK(SubroutineReturn, Operation);
    SOUFFLE_VISITOR_LINK(UnpackRecord, TupleOperation);
    SOUFFLE_VISITOR_LINK(NestedIntrinsicOperator, TupleOperation)
    SOUFFLE_VISITOR_LINK(Scan, RelationOperation);
    SOUFFLE_VISITOR_LINK(ParallelScan, Scan);
    SOUFFLE_VISITOR_LINK(IndexScan, IndexOperation);
    SOUFFLE_VISITOR_LINK(ParallelIndexScan, IndexScan);
    SOUFFLE_VISITOR_LINK(IfExists, RelationOperation);
    SOUFFLE_VISITOR_LINK(ParallelIfExists, IfExists);
    SOUFFLE_VISITOR_LINK(IndexIfExists, IndexOperation);
    SOUFFLE_VISITOR_LINK(ParallelIndexIfExists, IndexIfExists);
    SOUFFLE_VISITOR_LINK(RelationOperation, TupleOperation);
    SOUFFLE_VISITOR_LINK(Aggregate, RelationOperation);
    SOUFFLE_VISITOR_LINK(ParallelAggregate, Aggregate);
    SOUFFLE_VISITOR_LINK(IndexAggregate, IndexOperation);
    SOUFFLE_VISITOR_LINK(ParallelIndexAggregate, IndexAggregate);
    SOUFFLE_VISITOR_LINK(IndexOperation, RelationOperation);
    SOUFFLE_VISITOR_LINK(TupleOperation, NestedOperation);
    SOUFFLE_VISITOR_LINK(Filter, AbstractConditional);
    SOUFFLE_VISITOR_LINK(Break, AbstractConditional);
    SOUFFLE_VISITOR_LINK(AbstractConditional, NestedOperation);
    SOUFFLE_VISITOR_LINK(NestedOperation, Operation);

    SOUFFLE_VISITOR_LINK(Operation, Node);

    // -- conditions --
    SOUFFLE_VISITOR_LINK(True, Condition);
    SOUFFLE_VISITOR_LINK(False, Condition);
    SOUFFLE_VISITOR_LINK(Conjunction, Condition);
    SOUFFLE_VISITOR_LINK(Negation, Condition);
    SOUFFLE_VISITOR_LINK(Constraint, Condition);
    SOUFFLE_VISITOR_LINK(ProvenanceExistenceCheck, AbstractExistenceCheck);
    SOUFFLE_VISITOR_LINK(ExistenceCheck, AbstractExistenceCheck);
    SOUFFLE_VISITOR_LINK(EmptinessCheck, Condition);
    SOUFFLE_VISITOR_LINK(AbstractExistenceCheck, Condition);

    SOUFFLE_VISITOR_LINK(Condition, Node);

    // -- values --
    SOUFFLE_VISITOR_LINK(SignedConstant, NumericConstant);
    SOUFFLE_VISITOR_LINK(UnsignedConstant, NumericConstant);
    SOUFFLE_VISITOR_LINK(FloatConstant, NumericConstant);
    SOUFFLE_VISITOR_LINK(NumericConstant, Expression);
    SOUFFLE_VISITOR_LINK(StringConstant, Expression);
    SOUFFLE_VISITOR_LINK(UndefValue, Expression);
    SOUFFLE_VISITOR_LINK(TupleElement, Expression);
    SOUFFLE_VISITOR_LINK(IntrinsicOperator, AbstractOperator);
    SOUFFLE_VISITOR_LINK(UserDefinedOperator, AbstractOperator);
    SOUFFLE_VISITOR_LINK(AbstractOperator, Expression);
    SOUFFLE_VISITOR_LINK(AutoIncrement, Expression);
    SOUFFLE_VISITOR_LINK(PackRecord, Expression);
    SOUFFLE_VISITOR_LINK(SubroutineArgument, Expression);
    SOUFFLE_VISITOR_LINK(RelationSize, Expression);

    SOUFFLE_VISITOR_LINK(Expression, Node);

    // -- program --
    SOUFFLE_VISITOR_LINK(Program, Node);

    // -- relation
    SOUFFLE_VISITOR_LINK(Relation, Node);
};
}  // namespace souffle::ram

SOUFFLE_VISITOR_DEFINE_PARTIAL_SPECIALISATION(souffle::ram::Node, souffle::ram::Visitor);
