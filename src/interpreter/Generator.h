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
#include "interpreter/Engine.h"
#include "interpreter/Index.h"
#include "interpreter/Relation.h"
#include "interpreter/ViewContext.h"
#include "ram/AbstractExistenceCheck.h"
#include "ram/AbstractParallel.h"
#include "ram/Aggregate.h"
#include "ram/AutoIncrement.h"
#include "ram/Break.h"
#include "ram/Call.h"
#include "ram/Choice.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constant.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/Extend.h"
#include "ram/False.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/IndexAggregate.h"
#include "ram/IndexChoice.h"
#include "ram/IndexOperation.h"
#include "ram/IndexScan.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/PackRecord.h"
#include "ram/Parallel.h"
#include "ram/ParallelAggregate.h"
#include "ram/ParallelChoice.h"
#include "ram/ParallelIndexAggregate.h"
#include "ram/ParallelIndexChoice.h"
#include "ram/ParallelIndexScan.h"
#include "ram/ParallelScan.h"
#include "ram/Program.h"
#include "ram/Project.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/RelationSize.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/Statement.h"
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

/*
 * @class NodeGenerator
 * @brief Generate an executable Node tree based on the RAM tree.
 *        Each node contains run time information which is necessary for Engine to interpreter.
 */
class NodeGenerator : public ram::Visitor<Own<Node>> {
    using NodePtr = Own<Node>;
    using NodePtrVec = std::vector<NodePtr>;
    using RelationHandle = Own<RelationWrapper>;

public:
    NodeGenerator(Engine& engine) : engine(engine) {
        visitDepthFirst(engine.tUnit.getProgram(), [&](const ram::Relation& relation) {
            assert(relationMap.find(relation.getName()) == relationMap.end() && "double-naming of relations");
            relationMap[relation.getName()] = &relation;
        });
    }

    /**
     * @brief Generate the tree based on given entry.
     * Return a NodePtr to the root.
     */
    NodePtr generateTree(const ram::Node& root) {
        // Encode all relation, indexPos and viewId.
        visitDepthFirst(root, [&](const ram::Node& node) {
            if (isA<ram::Query>(&node)) {
                newQueryBlock();
            }
            if (const auto* indexSearch = dynamic_cast<const ram::IndexOperation*>(&node)) {
                encodeIndexPos(*indexSearch);
                encodeView(indexSearch);
            } else if (const auto* exists = dynamic_cast<const ram::ExistenceCheck*>(&node)) {
                encodeIndexPos(*exists);
                encodeView(exists);
            } else if (const auto* provExists = dynamic_cast<const ram::ProvenanceExistenceCheck*>(&node)) {
                encodeIndexPos(*provExists);
                encodeView(provExists);
            }
        });
        // Parse program
        return visit(root);
    }

    NodePtr visitConstant(const ram::Constant& num) override {
        return mk<Constant>(I_Constant, &num);
    }

    NodePtr visitTupleElement(const ram::TupleElement& access) override {
        auto tupleId = access.getTupleId();
        auto elementId = access.getElement();
        auto newElementId = orderingContext.mapOrder(tupleId, elementId);
        return mk<TupleElement>(I_TupleElement, &access, tupleId, newElementId);
    }

    NodePtr visitAutoIncrement(const ram::AutoIncrement& inc) override {
        return mk<AutoIncrement>(I_AutoIncrement, &inc);
    }

    NodePtr visitIntrinsicOperator(const ram::IntrinsicOperator& op) override {
        NodePtrVec children;
        for (const auto& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        return mk<IntrinsicOperator>(I_IntrinsicOperator, &op, std::move(children));
    }

    NodePtr visitUserDefinedOperator(const ram::UserDefinedOperator& op) override {
        NodePtrVec children;
        for (const auto& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        return mk<UserDefinedOperator>(I_UserDefinedOperator, &op, std::move(children));
    }

    NodePtr visitNestedIntrinsicOperator(const ram::NestedIntrinsicOperator& op) override {
        auto arity = op.getArguments().size();
        orderingContext.addNewTuple(op.getTupleId(), arity);
        NodePtrVec children;
        for (auto&& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        children.push_back(visitTupleOperation(op));
        return mk<NestedIntrinsicOperator>(I_NestedIntrinsicOperator, &op, std::move(children));
    }

    NodePtr visitPackRecord(const ram::PackRecord& pr) override {
        NodePtrVec children;
        for (const auto& arg : pr.getArguments()) {
            children.push_back(visit(arg));
        }
        return mk<PackRecord>(I_PackRecord, &pr, std::move(children));
    }

    NodePtr visitSubroutineArgument(const ram::SubroutineArgument& arg) override {
        return mk<SubroutineArgument>(I_SubroutineArgument, &arg);
    }

    // -- connectors operators --
    NodePtr visitTrue(const ram::True& ltrue) override {
        return mk<True>(I_True, &ltrue);
    }

    NodePtr visitFalse(const ram::False& lfalse) override {
        return mk<False>(I_False, &lfalse);
    }

    NodePtr visitConjunction(const ram::Conjunction& conj) override {
        return mk<Conjunction>(I_Conjunction, &conj, visit(conj.getLHS()), visit(conj.getRHS()));
    }

    NodePtr visitNegation(const ram::Negation& neg) override {
        return mk<Negation>(I_Negation, &neg, visit(neg.getOperand()));
    }

    NodePtr visitEmptinessCheck(const ram::EmptinessCheck& emptiness) override {
        size_t relId = encodeRelation(emptiness.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("EmptinessCheck", lookup(emptiness.getRelation()));
        return mk<EmptinessCheck>(type, &emptiness, rel);
    }

    NodePtr visitRelationSize(const ram::RelationSize& size) override {
        size_t relId = encodeRelation(size.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("RelationSize", lookup(size.getRelation()));
        return mk<RelationSize>(type, &size, rel);
    }

    NodePtr visitExistenceCheck(const ram::ExistenceCheck& exists) override {
        SuperInstruction superOp = getExistenceSuperInstInfo(exists);
        // Check if the search signature is a total signature
        bool isTotal = true;
        for (const auto& cur : exists.getValues()) {
            if (isUndefValue(cur)) {
                isTotal = false;
            }
        }
        auto ramRelation = lookup(exists.getRelation());
        NodeType type = constructNodeType("ExistenceCheck", ramRelation);
        return mk<ExistenceCheck>(type, &exists, isTotal, encodeView(&exists), std::move(superOp),
                ramRelation.isTemp(), ramRelation.getName());
    }

    NodePtr visitProvenanceExistenceCheck(const ram::ProvenanceExistenceCheck& provExists) override {
        SuperInstruction superOp = getExistenceSuperInstInfo(provExists);
        NodeType type = constructNodeType("ProvenanceExistenceCheck", lookup(provExists.getRelation()));
        return mk<ProvenanceExistenceCheck>(type, &provExists, visit(provExists.getChildNodes().back()),
                encodeView(&provExists), std::move(superOp));
    }

    // -- comparison operators --
    NodePtr visitConstraint(const ram::Constraint& relOp) override {
        return mk<Constraint>(I_Constraint, &relOp, visit(relOp.getLHS()), visit(relOp.getRHS()));
    }

    NodePtr visitNestedOperation(const ram::NestedOperation& nested) override {
        return visit(nested.getOperation());
    }

    NodePtr visitTupleOperation(const ram::TupleOperation& search) override {
        if (engine.profileEnabled) {
            return mk<TupleOperation>(I_TupleOperation, &search, visit(search.getOperation()));
        }
        return visit(search.getOperation());
    }

    NodePtr visitScan(const ram::Scan& scan) override {
        orderingContext.addTupleWithDefaultOrder(scan.getTupleId(), scan);
        size_t relId = encodeRelation(scan.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("Scan", lookup(scan.getRelation()));
        return mk<Scan>(type, &scan, rel, visitTupleOperation(scan));
    }

    NodePtr visitParallelScan(const ram::ParallelScan& pScan) override {
        orderingContext.addTupleWithDefaultOrder(pScan.getTupleId(), pScan);
        size_t relId = encodeRelation(pScan.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("ParallelScan", lookup(pScan.getRelation()));
        auto res = mk<ParallelScan>(type, &pScan, rel, visitTupleOperation(pScan));
        res->setViewContext(parentQueryViewContext);
        return res;
    }

    NodePtr visitIndexScan(const ram::IndexScan& iScan) override {
        orderingContext.addTupleWithIndexOrder(iScan.getTupleId(), iScan);
        SuperInstruction indexOperation = getIndexSuperInstInfo(iScan);
        NodeType type = constructNodeType("IndexScan", lookup(iScan.getRelation()));
        return mk<IndexScan>(type, &iScan, nullptr, visitTupleOperation(iScan), encodeView(&iScan),
                std::move(indexOperation));
    }

    NodePtr visitParallelIndexScan(const ram::ParallelIndexScan& piscan) override {
        orderingContext.addTupleWithIndexOrder(piscan.getTupleId(), piscan);
        SuperInstruction indexOperation = getIndexSuperInstInfo(piscan);
        size_t relId = encodeRelation(piscan.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("ParallelIndexScan", lookup(piscan.getRelation()));
        auto res = mk<ParallelIndexScan>(type, &piscan, rel, visitTupleOperation(piscan),
                encodeIndexPos(piscan), std::move(indexOperation));
        res->setViewContext(parentQueryViewContext);
        return res;
    }

    NodePtr visitChoice(const ram::Choice& choice) override {
        orderingContext.addTupleWithDefaultOrder(choice.getTupleId(), choice);
        size_t relId = encodeRelation(choice.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("Choice", lookup(choice.getRelation()));
        return mk<Choice>(type, &choice, rel, visit(choice.getCondition()), visitTupleOperation(choice));
    }

    NodePtr visitParallelChoice(const ram::ParallelChoice& pChoice) override {
        orderingContext.addTupleWithDefaultOrder(pChoice.getTupleId(), pChoice);
        size_t relId = encodeRelation(pChoice.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("ParallelChoice", lookup(pChoice.getRelation()));
        auto res = mk<ParallelChoice>(
                type, &pChoice, rel, visit(pChoice.getCondition()), visitTupleOperation(pChoice));
        res->setViewContext(parentQueryViewContext);
        return res;
    }

    NodePtr visitIndexChoice(const ram::IndexChoice& iChoice) override {
        orderingContext.addTupleWithIndexOrder(iChoice.getTupleId(), iChoice);
        SuperInstruction indexOperation = getIndexSuperInstInfo(iChoice);
        NodeType type = constructNodeType("IndexChoice", lookup(iChoice.getRelation()));
        return mk<IndexChoice>(type, &iChoice, nullptr, visit(iChoice.getCondition()),
                visitTupleOperation(iChoice), encodeView(&iChoice), std::move(indexOperation));
    }

    NodePtr visitParallelIndexChoice(const ram::ParallelIndexChoice& piChoice) override {
        orderingContext.addTupleWithIndexOrder(piChoice.getTupleId(), piChoice);
        SuperInstruction indexOperation = getIndexSuperInstInfo(piChoice);
        size_t relId = encodeRelation(piChoice.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("ParallelIndexChoice", lookup(piChoice.getRelation()));
        auto res = mk<ParallelIndexChoice>(type, &piChoice, rel, visit(piChoice.getCondition()),
                visit(piChoice.getOperation()), encodeIndexPos(piChoice), std::move(indexOperation));
        res->setViewContext(parentQueryViewContext);
        return res;
    }

    NodePtr visitUnpackRecord(const ram::UnpackRecord& unpack) override {  // get reference
        orderingContext.addNewTuple(unpack.getTupleId(), unpack.getArity());
        return mk<UnpackRecord>(
                I_UnpackRecord, &unpack, visit(unpack.getExpression()), visitTupleOperation(unpack));
    }

    NodePtr visitAggregate(const ram::Aggregate& aggregate) override {
        // Notice: Aggregate is sensitive to the visiting order of the subexprs in order to make
        // orderCtxt consistent. The order of visiting should be the same as the order of execution during
        // runtime.
        orderingContext.addTupleWithDefaultOrder(aggregate.getTupleId(), aggregate);
        NodePtr expr = visit(aggregate.getExpression());
        NodePtr cond = visit(aggregate.getCondition());
        orderingContext.addNewTuple(aggregate.getTupleId(), 1);
        NodePtr nested = visitTupleOperation(aggregate);
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("Aggregate", lookup(aggregate.getRelation()));
        return mk<Aggregate>(type, &aggregate, rel, std::move(expr), std::move(cond), std::move(nested));
    }

    NodePtr visitParallelAggregate(const ram::ParallelAggregate& pAggregate) override {
        orderingContext.addTupleWithDefaultOrder(pAggregate.getTupleId(), pAggregate);
        NodePtr expr = visit(pAggregate.getExpression());
        NodePtr cond = visit(pAggregate.getCondition());
        orderingContext.addNewTuple(pAggregate.getTupleId(), 1);
        NodePtr nested = visitTupleOperation(pAggregate);
        size_t relId = encodeRelation(pAggregate.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("ParallelAggregate", lookup(pAggregate.getRelation()));
        auto res = mk<ParallelAggregate>(
                type, &pAggregate, rel, std::move(expr), std::move(cond), std::move(nested));
        res->setViewContext(parentQueryViewContext);

        return res;
    }

    NodePtr visitIndexAggregate(const ram::IndexAggregate& iAggregate) override {
        orderingContext.addTupleWithIndexOrder(iAggregate.getTupleId(), iAggregate);
        SuperInstruction indexOperation = getIndexSuperInstInfo(iAggregate);
        NodePtr expr = visit(iAggregate.getExpression());
        NodePtr cond = visit(iAggregate.getCondition());
        orderingContext.addNewTuple(iAggregate.getTupleId(), 1);
        NodePtr nested = visitTupleOperation(iAggregate);
        size_t relId = encodeRelation(iAggregate.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("IndexAggregate", lookup(iAggregate.getRelation()));
        return mk<IndexAggregate>(type, &iAggregate, rel, std::move(expr), std::move(cond), std::move(nested),
                encodeView(&iAggregate), std::move(indexOperation));
    }

    NodePtr visitParallelIndexAggregate(const ram::ParallelIndexAggregate& piAggregate) override {
        orderingContext.addTupleWithIndexOrder(piAggregate.getTupleId(), piAggregate);
        SuperInstruction indexOperation = getIndexSuperInstInfo(piAggregate);
        NodePtr expr = visit(piAggregate.getExpression());
        NodePtr cond = visit(piAggregate.getCondition());
        orderingContext.addNewTuple(piAggregate.getTupleId(), 1);
        NodePtr nested = visitTupleOperation(piAggregate);
        size_t relId = encodeRelation(piAggregate.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("ParallelIndexAggregate", lookup(piAggregate.getRelation()));
        auto res = mk<ParallelIndexAggregate>(type, &piAggregate, rel, std::move(expr), std::move(cond),
                std::move(nested), encodeView(&piAggregate), std::move(indexOperation));
        res->setViewContext(parentQueryViewContext);
        return res;
    }

    NodePtr visitBreak(const ram::Break& breakOp) override {
        return mk<Break>(I_Break, &breakOp, visit(breakOp.getCondition()), visit(breakOp.getOperation()));
    }

    NodePtr visitFilter(const ram::Filter& filter) override {
        return mk<Filter>(I_Filter, &filter, visit(filter.getCondition()), visit(filter.getOperation()));
    }

    NodePtr visitProject(const ram::Project& project) override {
        SuperInstruction superOp = getProjectSuperInstInfo(project);
        size_t relId = encodeRelation(project.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("Project", lookup(project.getRelation()));
        return mk<Project>(type, &project, rel, std::move(superOp));
    }

    // -- return from subroutine --
    NodePtr visitSubroutineReturn(const ram::SubroutineReturn& ret) override {
        NodePtrVec children;
        for (const auto& value : ret.getValues()) {
            children.push_back(visit(value));
        }
        return mk<SubroutineReturn>(I_SubroutineReturn, &ret, std::move(children));
    }

    NodePtr visitSequence(const ram::Sequence& seq) override {
        NodePtrVec children;
        for (const auto& value : seq.getStatements()) {
            children.push_back(visit(value));
        }
        return mk<Sequence>(I_Sequence, &seq, std::move(children));
    }

    NodePtr visitParallel(const ram::Parallel& parallel) override {
        // Parallel statements are executed in sequence for now.
        NodePtrVec children;
        for (const auto& value : parallel.getStatements()) {
            children.push_back(visit(value));
        }
        return mk<Parallel>(I_Parallel, &parallel, std::move(children));
    }

    NodePtr visitLoop(const ram::Loop& loop) override {
        return mk<Loop>(I_Loop, &loop, visit(loop.getBody()));
    }

    NodePtr visitExit(const ram::Exit& exit) override {
        return mk<Exit>(I_Exit, &exit, visit(exit.getCondition()));
    }

    NodePtr visitCall(const ram::Call& call) override {
        // translate a subroutine name to an index
        // the index is used to identify the subroutine
        // in the interpreter. The index is stored in the
        // data array of the Node as the first
        // entry.
        auto subs = engine.tUnit.getProgram().getSubroutines();
        size_t subroutineId = distance(subs.begin(), subs.find(call.getName()));
        return mk<Call>(I_Call, &call, subroutineId);
    }

    NodePtr visitLogRelationTimer(const ram::LogRelationTimer& timer) override {
        size_t relId = encodeRelation(timer.getRelation());
        auto rel = getRelationHandle(relId);
        NodePtrVec children;
        children.push_back(visit(timer.getStatement()));
        return mk<LogRelationTimer>(I_LogRelationTimer, &timer, visit(timer.getStatement()), rel);
    }

    NodePtr visitLogTimer(const ram::LogTimer& timer) override {
        NodePtrVec children;
        children.push_back(visit(timer.getStatement()));
        return mk<LogTimer>(I_LogTimer, &timer, visit(timer.getStatement()));
    }

    NodePtr visitDebugInfo(const ram::DebugInfo& dbg) override {
        NodePtrVec children;
        children.push_back(visit(dbg.getStatement()));
        return mk<DebugInfo>(I_DebugInfo, &dbg, visit(dbg.getStatement()));
    }

    NodePtr visitClear(const ram::Clear& clear) override {
        size_t relId = encodeRelation(clear.getRelation());
        auto rel = getRelationHandle(relId);
        NodeType type = constructNodeType("Clear", lookup(clear.getRelation()));
        return mk<Clear>(type, &clear, rel);
    }

    NodePtr visitLogSize(const ram::LogSize& size) override {
        size_t relId = encodeRelation(size.getRelation());
        auto rel = getRelationHandle(relId);
        return mk<LogSize>(I_LogSize, &size, rel);
    }

    NodePtr visitIO(const ram::IO& io) override {
        size_t relId = encodeRelation(io.getRelation());
        auto rel = getRelationHandle(relId);
        return mk<IO>(I_IO, &io, rel);
    }

    NodePtr visitQuery(const ram::Query& query) override {
        std::shared_ptr<ViewContext> viewContext = std::make_shared<ViewContext>();
        parentQueryViewContext = viewContext;
        // split terms of conditions of outer-most filter operation
        // into terms that require a context and terms that
        // do not require a view
        const ram::Operation* next = &query.getOperation();
        std::vector<const ram::Condition*> freeOfView;
        if (const auto* filter = dynamic_cast<const ram::Filter*>(&query.getOperation())) {
            next = &filter->getOperation();
            // Check terms of outer filter operation whether they can be pushed before
            // the view-generation for speed improvements
            auto conditions = findConjunctiveTerms(&filter->getCondition());
            for (auto const& cur : conditions) {
                bool needView = false;
                visitDepthFirst(*cur, [&](const ram::Node& node) {
                    if (requireView(&node)) {
                        needView = true;
                        const auto& rel = getViewRelation(&node);
                        viewContext->addViewInfoForFilter(
                                encodeRelation(rel), indexTable[&node], encodeView(&node));
                    }
                });

                if (needView) {
                    viewContext->addViewOperationForFilter(visit(*cur));
                } else {
                    viewContext->addViewFreeOperationForFilter(visit(*cur));
                }
            }
        }

        visitDepthFirst(*next, [&](const ram::Node& node) {
            if (requireView(&node)) {
                const auto& rel = getViewRelation(&node);
                viewContext->addViewInfoForNested(encodeRelation(rel), indexTable[&node], encodeView(&node));
            };
        });

        visitDepthFirst(*next, [&](const ram::AbstractParallel&) { viewContext->isParallel = true; });

        auto res = mk<Query>(I_Query, &query, visit(*next));
        res->setViewContext(parentQueryViewContext);
        return res;
    }

    NodePtr visitExtend(const ram::Extend& extend) override {
        size_t src = encodeRelation(extend.getFirstRelation());
        size_t target = encodeRelation(extend.getSecondRelation());
        return mk<Extend>(I_Extend, &extend, src, target);
    }

    NodePtr visitSwap(const ram::Swap& swap) override {
        size_t src = encodeRelation(swap.getFirstRelation());
        size_t target = encodeRelation(swap.getSecondRelation());
        return mk<Swap>(I_Swap, &swap, src, target);
    }

    NodePtr visitUndefValue(const ram::UndefValue&) override {
        return nullptr;
    }

    NodePtr visitNode(const ram::Node& node) override {
        fatal("unsupported node type: %s", typeid(node).name());
    }

private:
    /**
     * @class OrderingContext
     * @brief This class help generator reordering tuple access based on the index oder.
     */
    class OrderingContext {
    public:
        OrderingContext(NodeGenerator& generator) : generator(generator) {}

        /** @brief Bind tuple with a natural full order.
         *
         * This is usually used when an operation implicitly introduce a runtime tuple, such as UnpackRecord
         * NestedIntrinsicOperator, and nested operation in Aggregate.
         * */
        void addNewTuple(size_t tupleId, size_t arity) {
            std::vector<uint32_t> order;
            for (size_t i = 0; i < arity; ++i) {
                order.push_back((uint32_t)i);
            }
            insertOrder(tupleId, std::move(order));
        }

        /** @brief Bind tuple with the default order.
         *
         * This is usually used for tuples created by non-indexed operations. Such as Scan, Aggregate, Choice.
         * */
        template <class RamNode>
        void addTupleWithDefaultOrder(size_t tupleId, const RamNode& node) {
            auto interpreterRel = generator.encodeRelation(node.getRelation());
            insertOrder(tupleId, (*generator.getRelationHandle(interpreterRel))->getIndexOrder(0));
        }

        /** @brief Bind tuple with the corresponding index order.
         *
         * This is usually used for tuples created by indexed operations. Such as IndexScan, IndexAggregate,
         * IndexChoice.
         * */
        template <class RamNode>
        void addTupleWithIndexOrder(size_t tupleId, const RamNode& node) {
            auto interpreterRel = generator.encodeRelation(node.getRelation());
            auto indexId = generator.encodeIndexPos(node);
            auto order = (*generator.getRelationHandle(interpreterRel))->getIndexOrder(indexId);
            insertOrder(tupleId, order);
        }

        /** @brief Map the decoded order of elementId based on current context */
        size_t mapOrder(size_t tupleId, size_t elementId) const {
            return tupleOrders[tupleId][elementId];
        }

    private:
        void insertOrder(size_t tupleId, const Order& order) {
            if (tupleId >= tupleOrders.size()) {
                tupleOrders.resize(tupleId + 1);
            }

            std::vector<uint32_t> decodeOrder(order.size());
            for (size_t i = 0; i < order.size(); ++i) {
                decodeOrder[order[i]] = i;
            }

            tupleOrders[tupleId] = std::move(decodeOrder);
        }
        std::vector<Order> tupleOrders;
        NodeGenerator& generator;
    };

private:
    /** @brief Reset view allocation system, since view's life time is within each query. */
    void newQueryBlock() {
        viewTable.clear();
        viewId = 0;
    }

    /** @brief Get a valid relation id for encoding */
    size_t getNewRelId() {
        return relId++;
    }

    /** @brief Get a valid view id for encoding */
    size_t getNextViewId() {
        return viewId++;
    }

    /** @brief Return operation index id from the result of indexAnalysis */
    template <class RamNode>
    size_t encodeIndexPos(RamNode& node) {
        const std::string& name = node.getRelation();
        auto& orderSet = engine.isa->getIndexes(name);
        ram::analysis::SearchSignature signature = engine.isa->getSearchSignature(&node);
        // A zero signature is equivalent as a full order signature.
        if (signature.empty()) {
            signature = ram::analysis::SearchSignature::getFullSearchSignature(signature.arity());
        }
        auto i = orderSet.getLexOrderNum(signature);
        indexTable[&node] = i;
        return i;
    };

    /** @brief Encode and return the View id of an operation. */
    size_t encodeView(const ram::Node* node) {
        auto pos = viewTable.find(node);
        if (pos != viewTable.end()) {
            return pos->second;
        }
        size_t id = getNextViewId();
        viewTable[node] = id;
        return id;
    }

    /** @brief get arity of relation */
    const ram::Relation& lookup(const std::string& relName) {
        auto it = relationMap.find(relName);
        assert(it != relationMap.end() && "relation not found");
        return *it->second;
    }

    /** @brief get arity of relation */
    size_t getArity(const std::string& relName) {
        auto rel = lookup(relName);
        return rel.getArity();
    }

    /** @brief Encode and create the relation, return the relation id */
    size_t encodeRelation(const std::string& relName) {
        auto pos = relTable.find(relName);
        if (pos != relTable.end()) {
            return pos->second;
        }
        size_t id = getNewRelId();
        relTable[relName] = id;
        engine.createRelation(lookup(relName), id);
        return id;
    }

    /* @brief Get a relation instance from engine */
    RelationHandle* getRelationHandle(const size_t idx) {
        return engine.relations[idx].get();
    }

    /**
     * Return true if the given operation requires a view.
     */
    bool requireView(const ram::Node* node) {
        if (isA<ram::AbstractExistenceCheck>(node)) {
            return true;
        } else if (isA<ram::IndexOperation>(node)) {
            return true;
        }
        return false;
    }

    /**
     * @brief Return the associated relation of a operation which requires a view.
     * This function assume the operation does requires a view.
     */
    const std::string& getViewRelation(const ram::Node* node) {
        if (const auto* exist = dynamic_cast<const ram::AbstractExistenceCheck*>(node)) {
            return exist->getRelation();
        } else if (const auto* index = dynamic_cast<const ram::IndexOperation*>(node)) {
            return index->getRelation();
        }

        fatal("The ram::Node does not require a view.");
    }

    /**
     * @brief Encode and return the super-instruction information about a index operation.
     */
    SuperInstruction getIndexSuperInstInfo(const ram::IndexOperation& ramIndex) {
        size_t arity = getArity(ramIndex.getRelation());
        auto interpreterRel = encodeRelation(ramIndex.getRelation());
        auto indexId = encodeIndexPos(ramIndex);
        auto order = (*getRelationHandle(interpreterRel))->getIndexOrder(indexId);
        SuperInstruction indexOperation(arity);
        const auto& first = ramIndex.getRangePattern().first;
        for (size_t i = 0; i < arity; ++i) {
            // Note: unlike orderingContext::mapOrder, where we try to decode the order,
            // here we have to encode the order.
            auto& low = first[order[i]];

            // Unbounded
            if (isUndefValue(low)) {
                indexOperation.first[i] = MIN_RAM_SIGNED;
                continue;
            }

            // Constant
            if (isA<ram::Constant>(low)) {
                indexOperation.first[i] = dynamic_cast<ram::Constant*>(low)->getConstant();
                continue;
            }

            // TupleElement
            if (isA<ram::TupleElement>(low)) {
                auto lowTuple = dynamic_cast<ram::TupleElement*>(low);
                size_t tupleId = lowTuple->getTupleId();
                size_t elementId = lowTuple->getElement();
                size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
                indexOperation.tupleFirst.push_back({i, tupleId, newElementId});
                continue;
            }

            // Generic expression
            indexOperation.exprFirst.push_back(std::pair<size_t, Own<Node>>(i, visit(low)));
        }
        const auto& second = ramIndex.getRangePattern().second;
        for (size_t i = 0; i < arity; ++i) {
            auto& hig = second[order[i]];

            // Unbounded
            if (isUndefValue(hig)) {
                indexOperation.second[i] = MAX_RAM_SIGNED;
                continue;
            }

            // Constant
            if (isA<ram::Constant>(hig)) {
                indexOperation.second[i] = dynamic_cast<ram::Constant*>(hig)->getConstant();
                continue;
            }

            // TupleElement
            if (isA<ram::TupleElement>(hig)) {
                auto highTuple = dynamic_cast<ram::TupleElement*>(hig);
                size_t tupleId = highTuple->getTupleId();
                size_t elementId = highTuple->getElement();
                size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
                indexOperation.tupleSecond.push_back({i, tupleId, newElementId});
                continue;
            }

            // Generic expression
            indexOperation.exprSecond.push_back(std::pair<size_t, Own<Node>>(i, visit(hig)));
        }
        return indexOperation;
    }

    /**
     * @brief Encode and return the super-instruction information about an existence check operation
     */
    SuperInstruction getExistenceSuperInstInfo(const ram::AbstractExistenceCheck& abstractExist) {
        auto interpreterRel = encodeRelation(abstractExist.getRelation());
        size_t indexId = 0;
        if (isA<ram::ExistenceCheck>(&abstractExist)) {
            indexId = encodeIndexPos(*dynamic_cast<const ram::ExistenceCheck*>(&abstractExist));
        } else if (isA<ram::ProvenanceExistenceCheck>(&abstractExist)) {
            indexId = encodeIndexPos(*dynamic_cast<const ram::ProvenanceExistenceCheck*>(&abstractExist));
        } else {
            fatal("Unrecognized ram::AbstractExistenceCheck.");
        }
        auto order = (*getRelationHandle(interpreterRel))->getIndexOrder(indexId);
        size_t arity = getArity(abstractExist.getRelation());
        SuperInstruction superOp(arity);
        const auto& children = abstractExist.getValues();
        for (size_t i = 0; i < arity; ++i) {
            auto& child = children[order[i]];

            // Unbounded
            if (isUndefValue(child)) {
                superOp.first[i] = MIN_RAM_SIGNED;
                superOp.second[i] = MAX_RAM_SIGNED;
                continue;
            }

            // Constant
            if (isA<ram::Constant>(child)) {
                superOp.first[i] = dynamic_cast<ram::Constant*>(child)->getConstant();
                superOp.second[i] = superOp.first[i];
                continue;
            }

            // TupleElement
            if (isA<ram::TupleElement>(child)) {
                auto tuple = dynamic_cast<ram::TupleElement*>(child);
                size_t tupleId = tuple->getTupleId();
                size_t elementId = tuple->getElement();
                size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
                superOp.tupleFirst.push_back({i, tupleId, newElementId});
                continue;
            }

            // Generic expression
            superOp.exprFirst.push_back(std::pair<size_t, Own<Node>>(i, visit(child)));
        }
        return superOp;
    }

    /**
     * @brief Encode and return the super-instruction information about a project operation
     *
     * No reordering needed for projection as project can have more then one target indexes and reordering can
     * only be done during runtime.
     */
    SuperInstruction getProjectSuperInstInfo(const ram::Project& exist) {
        size_t arity = getArity(exist.getRelation());
        SuperInstruction superOp(arity);
        const auto& children = exist.getValues();
        for (size_t i = 0; i < arity; ++i) {
            auto& child = children[i];
            // Constant
            if (isA<ram::Constant>(child)) {
                superOp.first[i] = dynamic_cast<ram::Constant*>(child)->getConstant();
                continue;
            }

            // TupleElement
            if (isA<ram::TupleElement>(child)) {
                auto tuple = dynamic_cast<ram::TupleElement*>(child);
                size_t tupleId = tuple->getTupleId();
                size_t elementId = tuple->getElement();
                size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
                superOp.tupleFirst.push_back({i, tupleId, newElementId});
                continue;
            }

            // Generic expression
            superOp.exprFirst.push_back(std::pair<size_t, Own<Node>>(i, visit(child)));
        }
        return superOp;
    }

    /** Environment encoding, store a mapping from ram::Node to its operation index id. */
    std::unordered_map<const ram::Node*, size_t> indexTable;
    /** Points to the current viewContext during the generation.
     * It is used to passing viewContext between parent query and its nested parallel operation.
     * As parallel operation requires its own view information. */
    std::shared_ptr<ViewContext> parentQueryViewContext = nullptr;
    /** Next available location to encode View */
    size_t viewId = 0;
    /** Next available location to encode a relation */
    size_t relId = 0;
    /** Environment encoding, store a mapping from ram::Node to its View id. */
    std::unordered_map<const ram::Node*, size_t> viewTable;
    /** Environment encoding, store a mapping from ram::Relation to its id */
    std::unordered_map<std::string, size_t> relTable;
    /** name / relation mapping */
    std::unordered_map<std::string, const ram::Relation*> relationMap;
    /** ordering context */
    OrderingContext orderingContext = OrderingContext(*this);
    /** Reference to the engine instance */
    Engine& engine;
};
}  // namespace souffle::interpreter
