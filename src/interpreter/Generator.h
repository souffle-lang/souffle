/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Generator.h
 *
 * Declares the Interpreter Generator class. The generator takes an entry
 * of the RAM program and translate it into an executable Node representation
 * with environment symbol binding in each node.
 ***********************************************************************/

#pragma once

#include "Global.h"
#include "RelationTag.h"
#include "interpreter/Index.h"
#include "interpreter/Relation.h"
#include "interpreter/ViewContext.h"
#include "ram/AbstractExistenceCheck.h"
#include "ram/AbstractParallel.h"
#include "ram/Aggregate.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/CountUniqueKeys.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/IfExists.h"
#include "ram/IfNotExists.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexIfExists.h"
#include "ram/IndexOperation.h"
#include "ram/IndexScan.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/IterationNumber.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/MergeExtend.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/NumericConstant.h"
#include "ram/Operation.h"
#include "ram/OperationSequence.h"
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
#include "ram/RelationSize.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
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
#include "ram/UserDefinedOperator.h"
#include "ram/analysis/Index.h"
#include "ram/utility/Utils.h"
#include "ram/utility/Visitor.h"
#include "souffle/RamTypes.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace souffle::interpreter {
class Engine;
/*
 * @class NodeGenerator
 * @brief Generate an executable Node tree based on the RAM tree.
 *        Each node contains run time information which is necessary for Engine to interpreter.
 */
class NodeGenerator : public ram::Visitor<Own<Node>> {
    using NodePtr = Own<Node>;
    using NodePtrVec = std::vector<NodePtr>;
    using RelationHandle = Own<RelationWrapper>;
    using ram::Visitor<Own<Node>>::visit_;

public:
    NodeGenerator(Engine& engine);

    /**
     * @brief Generate the tree based on given entry.
     * Return a NodePtr to the root.
     */
    NodePtr generateTree(const ram::Node& root);

    NodePtr visit_(type_identity<ram::NumericConstant>, const ram::NumericConstant& num) override;

    NodePtr visit_(type_identity<ram::StringConstant>, const ram::StringConstant& num) override;

    NodePtr visit_(type_identity<ram::TupleElement>, const ram::TupleElement& access) override;

    NodePtr visit_(type_identity<ram::AutoIncrement>, const ram::AutoIncrement& inc) override;

    NodePtr visit_(type_identity<ram::IterationNumber>, const ram::IterationNumber& inc) override;

    NodePtr visit_(type_identity<ram::IntrinsicOperator>, const ram::IntrinsicOperator& op) override;

    NodePtr visit_(type_identity<ram::UserDefinedOperator>, const ram::UserDefinedOperator& op) override;

    NodePtr visit_(
            type_identity<ram::NestedIntrinsicOperator>, const ram::NestedIntrinsicOperator& op) override;

    NodePtr visit_(type_identity<ram::PackRecord>, const ram::PackRecord& pr) override;

    NodePtr visit_(type_identity<ram::SubroutineArgument>, const ram::SubroutineArgument& arg) override;

    NodePtr visit_(type_identity<ram::True>, const ram::True& ltrue) override;

    NodePtr visit_(type_identity<ram::False>, const ram::False& lfalse) override;

    NodePtr visit_(type_identity<ram::Conjunction>, const ram::Conjunction& conj) override;

    NodePtr visit_(type_identity<ram::Negation>, const ram::Negation& neg) override;

    NodePtr visit_(type_identity<ram::EmptinessCheck>, const ram::EmptinessCheck& emptiness) override;

    NodePtr visit_(type_identity<ram::RelationSize>, const ram::RelationSize& size) override;

    NodePtr visit_(type_identity<ram::ExistenceCheck>, const ram::ExistenceCheck& exists) override;

    NodePtr visit_(type_identity<ram::ProvenanceExistenceCheck>,
            const ram::ProvenanceExistenceCheck& provExists) override;

    NodePtr visit_(type_identity<ram::Constraint>, const ram::Constraint& relOp) override;

    NodePtr visit_(type_identity<ram::NestedOperation>, const ram::NestedOperation& nested) override;

    NodePtr visit_(type_identity<ram::TupleOperation>, const ram::TupleOperation& search) override;

    NodePtr visit_(type_identity<ram::OperationSequence>, const ram::OperationSequence& opers) override;

    NodePtr visit_(type_identity<ram::Scan>, const ram::Scan& scan) override;

    NodePtr visit_(type_identity<ram::ParallelScan>, const ram::ParallelScan& pScan) override;

    NodePtr visit_(type_identity<ram::IndexScan>, const ram::IndexScan& iScan) override;

    NodePtr visit_(type_identity<ram::ParallelIndexScan>, const ram::ParallelIndexScan& piscan) override;

    NodePtr visit_(type_identity<ram::IfExists>, const ram::IfExists& ifexists) override;

    NodePtr visit_(type_identity<ram::IfNotExists>, const ram::IfNotExists& ifexists) override;

    NodePtr visit_(type_identity<ram::ParallelIfExists>, const ram::ParallelIfExists& pIfExists) override;

    NodePtr visit_(type_identity<ram::IndexIfExists>, const ram::IndexIfExists& iIfExists) override;

    NodePtr visit_(
            type_identity<ram::ParallelIndexIfExists>, const ram::ParallelIndexIfExists& piIfExists) override;

    NodePtr visit_(type_identity<ram::UnpackRecord>, const ram::UnpackRecord& unpack) override;

    NodePtr visit_(type_identity<ram::Aggregate>, const ram::Aggregate& aggregate) override;

    NodePtr visit_(type_identity<ram::ParallelAggregate>, const ram::ParallelAggregate& pAggregate) override;

    NodePtr visit_(type_identity<ram::IndexAggregate>, const ram::IndexAggregate& iAggregate) override;

    NodePtr visit_(type_identity<ram::ParallelIndexAggregate>,
            const ram::ParallelIndexAggregate& piAggregate) override;

    NodePtr visit_(type_identity<ram::Break>, const ram::Break& breakOp) override;

    NodePtr visit_(type_identity<ram::Filter>, const ram::Filter& filter) override;

    NodePtr visit_(type_identity<ram::GuardedInsert>, const ram::GuardedInsert& guardedPorject) override;

    NodePtr visit_(type_identity<ram::Insert>, const ram::Insert& insert) override;

    NodePtr visit_(type_identity<ram::Erase>, const ram::Erase& erase) override;

    NodePtr visit_(type_identity<ram::SubroutineReturn>, const ram::SubroutineReturn& ret) override;

    NodePtr visit_(type_identity<ram::Sequence>, const ram::Sequence& seq) override;

    NodePtr visit_(type_identity<ram::Parallel>, const ram::Parallel& parallel) override;

    NodePtr visit_(type_identity<ram::Loop>, const ram::Loop& loop) override;

    NodePtr visit_(type_identity<ram::Exit>, const ram::Exit& exit) override;

    NodePtr visit_(type_identity<ram::Call>, const ram::Call& call) override;

    NodePtr visit_(type_identity<ram::LogRelationTimer>, const ram::LogRelationTimer& timer) override;

    NodePtr visit_(type_identity<ram::LogTimer>, const ram::LogTimer& timer) override;

    NodePtr visit_(type_identity<ram::DebugInfo>, const ram::DebugInfo& dbg) override;

    NodePtr visit_(type_identity<ram::Clear>, const ram::Clear& clear) override;

    NodePtr visit_(type_identity<ram::CountUniqueKeys>, const ram::CountUniqueKeys& count) override;

    NodePtr visit_(type_identity<ram::LogSize>, const ram::LogSize& size) override;

    NodePtr visit_(type_identity<ram::IO>, const ram::IO& io) override;

    NodePtr visit_(type_identity<ram::Query>, const ram::Query& query) override;

    NodePtr visit_(type_identity<ram::MergeExtend>, const ram::MergeExtend& extend) override;

    NodePtr visit_(type_identity<ram::Swap>, const ram::Swap& swap) override;

    NodePtr visit_(type_identity<ram::UndefValue>, const ram::UndefValue&) override;

    NodePtr visit_(type_identity<ram::Node>, const ram::Node& node) override;

private:
    /**
     * @class OrderingContext
     * @brief This class help generator reordering tuple access based on the index oder.
     */
    class OrderingContext {
    public:
        OrderingContext(NodeGenerator& generator);

        /** @brief Bind tuple with a natural full order.
         *
         * This is usually used when an operation implicitly introduce a runtime tuple, such as UnpackRecord
         * NestedIntrinsicOperator, and nested operation in Aggregate.
         * */
        void addNewTuple(std::size_t tupleId, std::size_t arity);

        /** @brief Bind tuple with the default order.
         *
         * This is usually used for tuples created by non-indexed operations. Such as Scan, Aggregate,
         * IfExists.
         * */
        template <class RamNode>
        void addTupleWithDefaultOrder(std::size_t tupleId, const RamNode& node);

        /** @brief Bind tuple with the corresponding index order.
         *
         * This is usually used for tuples created by indexed operations. Such as IndexScan, IndexAggregate,
         * IndexIfExists.
         * */
        template <class RamNode>
        void addTupleWithIndexOrder(std::size_t tupleId, const RamNode& node);

        /** @brief Map the decoded order of elementId based on current context */
        std::size_t mapOrder(std::size_t tupleId, std::size_t elementId) const;

    private:
        void insertOrder(std::size_t tupleId, const Order& order);
        std::vector<Order> tupleOrders;
        NodeGenerator& generator;
    };

private:
    /** @brief Reset view allocation system, since view's life time is within each query. */
    void newQueryBlock();

    /** @brief Get a valid relation id for encoding */
    std::size_t getNewRelId();

    /** @brief Get a valid view id for encoding */
    std::size_t getNextViewId();

    /** @brief Return operation index id from the result of indexAnalysis */
    template <class RamNode>
    std::size_t encodeIndexPos(RamNode& node);

    /** @brief Encode and return the View id of an operation. */
    std::size_t encodeView(const ram::Node* node);

    /** @brief get arity of relation */
    const ram::Relation& lookup(const std::string& relName);

    /** @brief get arity of relation */
    std::size_t getArity(const std::string& relName);

    /** @brief Encode and create the relation, return the relation id */
    std::size_t encodeRelation(const std::string& relName);

    /* @brief Get a relation instance from engine */
    RelationHandle* getRelationHandle(const std::size_t idx);

    /**
     * Return true if the given operation requires a view.
     */
    bool requireView(const ram::Node* node);

    /**
     * @brief Return the associated relation of a operation which requires a view.
     * This function assume the operation does requires a view.
     */
    const std::string& getViewRelation(const ram::Node* node);

    /**
     * @brief Encode and return the super-instruction information about a index operation.
     */
    SuperInstruction getIndexSuperInstInfo(const ram::IndexOperation& ramIndex);

    /**
     * @brief Encode and return the super-instruction information about an existence check operation
     */
    SuperInstruction getExistenceSuperInstInfo(const ram::AbstractExistenceCheck& abstractExist);

    /**
     * @brief Encode and return the super-instruction information about a insert operation
     *
     * No reordering needed for insertion as insert can have more then one target indexes and reordering can
     * only be done during runtime.
     */
    SuperInstruction getInsertSuperInstInfo(const ram::Insert& exist);
    SuperInstruction getEraseSuperInstInfo(const ram::Erase& exist);

    /** Environment encoding, store a mapping from ram::Node to its operation index id. */
    std::unordered_map<const ram::Node*, std::size_t> indexTable;
    /** Points to the current viewContext during the generation.
     * It is used to passing viewContext between parent query and its nested parallel operation.
     * As parallel operation requires its own view information. */
    std::shared_ptr<ViewContext> parentQueryViewContext = nullptr;
    /** Next available location to encode View */
    std::size_t viewId = 0;
    /** Next available location to encode a relation */
    std::size_t relId = 0;
    /** Environment encoding, store a mapping from ram::Node to its View id. */
    std::unordered_map<const ram::Node*, std::size_t> viewTable;
    /** Environment encoding, store a mapping from ram::Relation to its id */
    std::unordered_map<std::string, std::size_t> relTable;
    /** name / relation mapping */
    std::unordered_map<std::string, const ram::Relation*> relationMap;
    /** ordering context */
    OrderingContext orderingContext = OrderingContext(*this);
    /** Reference to the engine instance */
    Engine& engine;
};
}  // namespace souffle::interpreter
