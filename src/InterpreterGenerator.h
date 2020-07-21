/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterGenerator.h
 *
 * Declares the Interpreter Generator class. The generator takes an entry
 * of the RAM program and translate it into an executable InterpreterNode representation
 * with environment symbol binding in each node.
 ***********************************************************************/

#pragma once

#include "Global.h"
#include "InterpreterIndex.h"
#include "InterpreterNode.h"
#include "InterpreterPreamble.h"
#include "InterpreterRelation.h"
#include "RamTypes.h"
#include "RelationTag.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include "ram/RamUtils.h"
#include "ram/RamVisitor.h"
#include "ram/analysis/RamIndexAnalysis.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <queue>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace souffle {

/*
 * @class NodeGenerator
 * @brief Generate an executable InterpreterNode tree based on the RAM tree.
 *        Each node contains run time information which is necessary for InterpreterEngine to interpreter.
 */
class NodeGenerator : public RamVisitor<std::unique_ptr<InterpreterNode>> {
    using NodePtr = std::unique_ptr<InterpreterNode>;
    using NodePtrVec = std::vector<NodePtr>;
    using RelationHandle = std::unique_ptr<InterpreterRelation>;

public:
    NodeGenerator(RamIndexAnalysis* isa) : isa(isa), isProvenance(Global::config().has("provenance")) {}

    /**
     * @brief Generate the tree based on given entry.
     * Return a NodePtr to the root.
     */
    NodePtr generateTree(const RamNode& root, const RamProgram& program) {
        this->program = const_cast<RamProgram*>(&program);
        // Encode all relation, indexPos and viewId.
        visitDepthFirst(root, [&](const RamNode& node) {
            if (dynamic_cast<const RamQuery*>(&node) != nullptr) {
                newQueryBlock();
            }
            if (const auto* indexSearch = dynamic_cast<const RamIndexOperation*>(&node)) {
                encodeIndexPos(*indexSearch);
                encodeView(indexSearch);
            } else if (const auto* exists = dynamic_cast<const RamExistenceCheck*>(&node)) {
                encodeIndexPos(*exists);
                encodeView(exists);
            } else if (const auto* provExists = dynamic_cast<const RamProvenanceExistenceCheck*>(&node)) {
                encodeIndexPos(*provExists);
                encodeView(provExists);
            }
        });
        // Parse program
        return visit(root);
    }

    NodePtr visitConstant(const RamConstant& num) override {
        return std::make_unique<InterpreterNode>(I_Constant, &num);
    }

    NodePtr visitTupleElement(const RamTupleElement& access) override {
        return std::make_unique<InterpreterNode>(I_TupleElement, &access);
    }

    NodePtr visitAutoIncrement(const RamAutoIncrement& inc) override {
        return std::make_unique<InterpreterNode>(I_AutoIncrement, &inc);
    }

    NodePtr visitIntrinsicOperator(const RamIntrinsicOperator& op) override {
        NodePtrVec children;
        for (const auto& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        return std::make_unique<InterpreterNode>(I_IntrinsicOperator, &op, std::move(children));
    }

    NodePtr visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
        NodePtrVec children;
        for (const auto& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        return std::make_unique<InterpreterNode>(I_UserDefinedOperator, &op, std::move(children));
    }

    NodePtr visitNestedIntrinsicOperator(const RamNestedIntrinsicOperator& op) override {
        NodePtrVec children;
        for (auto&& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        children.push_back(visitTupleOperation(op));
        return std::make_unique<InterpreterNode>(I_NestedIntrinsicOperator, &op, std::move(children));
    }

    NodePtr visitPackRecord(const RamPackRecord& pr) override {
        NodePtrVec children;
        for (const auto& arg : pr.getArguments()) {
            children.push_back(visit(arg));
        }
        return std::make_unique<InterpreterNode>(I_PackRecord, &pr, std::move(children));
    }

    NodePtr visitSubroutineArgument(const RamSubroutineArgument& arg) override {
        return std::make_unique<InterpreterNode>(I_SubroutineArgument, &arg);
    }

    // -- connectors operators --
    NodePtr visitTrue(const RamTrue& ltrue) override {
        return std::make_unique<InterpreterNode>(I_True, &ltrue);
    }

    NodePtr visitFalse(const RamFalse& lfalse) override {
        return std::make_unique<InterpreterNode>(I_False, &lfalse);
    }

    NodePtr visitConjunction(const RamConjunction& conj) override {
        NodePtrVec children;
        children.push_back(visit(conj.getLHS()));
        children.push_back(visit(conj.getRHS()));
        return std::make_unique<InterpreterNode>(I_Conjunction, &conj, std::move(children));
    }

    NodePtr visitNegation(const RamNegation& neg) override {
        NodePtrVec children;
        children.push_back(visit(neg.getOperand()));
        return std::make_unique<InterpreterNode>(I_Negation, &neg, std::move(children));
    }

    NodePtr visitEmptinessCheck(const RamEmptinessCheck& emptiness) override {
        size_t relId = encodeRelation(emptiness.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterNode>(I_EmptinessCheck, &emptiness, NodePtrVec{}, rel);
    }

    NodePtr visitExistenceCheck(const RamExistenceCheck& exists) override {
        NodePtrVec children;
        for (const auto& value : exists.getValues()) {
            children.push_back(visit(value));
        }
        std::vector<size_t> data;
        data.push_back(encodeView(&exists));
        // Check if the search signature is a total signature
        size_t isTotal = 1;
        for (const auto& cur : exists.getValues()) {
            if (isRamUndefValue(cur)) {
                isTotal = 0;
            }
        }
        data.push_back(isTotal);
        return std::make_unique<InterpreterNode>(
                I_ExistenceCheck, &exists, std::move(children), nullptr, std::move(data));
    }

    NodePtr visitProvenanceExistenceCheck(const RamProvenanceExistenceCheck& provExists) override {
        NodePtrVec children;
        for (const auto& value : provExists.getValues()) {
            children.push_back(visit(value));
        }
        std::vector<size_t> data;
        data.push_back(encodeView(&provExists));
        return std::make_unique<InterpreterNode>(
                I_ProvenanceExistenceCheck, &provExists, std::move(children), nullptr, std::move(data));
    }

    // -- comparison operators --
    NodePtr visitConstraint(const RamConstraint& relOp) override {
        NodePtrVec children;
        children.push_back(visit(relOp.getLHS()));
        children.push_back(visit(relOp.getRHS()));
        return std::make_unique<InterpreterNode>(I_Constraint, &relOp, std::move(children));
    }

    NodePtr visitNestedOperation(const RamNestedOperation& nested) override {
        return visit(nested.getOperation());
    }

    NodePtr visitTupleOperation(const RamTupleOperation& search) override {
        NodePtrVec children;
        children.push_back(visit(search.getOperation()));
        return std::make_unique<InterpreterNode>(I_TupleOperation, &search, std::move(children));
    }

    NodePtr visitScan(const RamScan& scan) override {
        size_t relId = encodeRelation(scan.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visitTupleOperation(scan));
        return std::make_unique<InterpreterNode>(I_Scan, &scan, std::move(children), rel);
    }

    NodePtr visitParallelScan(const RamParallelScan& pScan) override {
        size_t relId = encodeRelation(pScan.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visitTupleOperation(pScan));
        auto res = std::make_unique<InterpreterNode>(I_ParallelScan, &pScan, std::move(children), rel);
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitIndexScan(const RamIndexScan& scan) override {
        NodePtrVec children;
        for (const auto& value : scan.getRangePattern().first) {
            children.push_back(visit(value));
        }
        for (const auto& value : scan.getRangePattern().second) {
            children.push_back(visit(value));
        }
        children.push_back(visitTupleOperation(scan));
        std::vector<size_t> data;
        data.push_back((encodeView(&scan)));
        return std::make_unique<InterpreterNode>(
                I_IndexScan, &scan, std::move(children), nullptr, std::move(data));
    }

    NodePtr visitParallelIndexScan(const RamParallelIndexScan& piscan) override {
        size_t relId = encodeRelation(piscan.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        for (const auto& value : piscan.getRangePattern().first) {
            children.push_back(visit(value));
        }
        for (const auto& value : piscan.getRangePattern().second) {
            children.push_back(visit(value));
        }
        children.push_back(visitTupleOperation(piscan));
        std::vector<size_t> data;
        data.push_back((encodeIndexPos(piscan)));
        auto res = std::make_unique<InterpreterNode>(
                I_ParallelIndexScan, &piscan, std::move(children), rel, std::move(data));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitChoice(const RamChoice& choice) override {
        size_t relId = encodeRelation(choice.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visit(choice.getCondition()));
        children.push_back(visitTupleOperation(choice));
        return std::make_unique<InterpreterNode>(I_Choice, &choice, std::move(children), rel);
    }

    NodePtr visitParallelChoice(const RamParallelChoice& pchoice) override {
        size_t relId = encodeRelation(pchoice.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visit(pchoice.getCondition()));
        children.push_back(visitTupleOperation(pchoice));
        auto res = std::make_unique<InterpreterNode>(I_ParallelChoice, &pchoice, std::move(children), rel);
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitIndexChoice(const RamIndexChoice& choice) override {
        NodePtrVec children;
        for (const auto& value : choice.getRangePattern().first) {
            children.push_back(visit(value));
        }
        for (const auto& value : choice.getRangePattern().second) {
            children.push_back(visit(value));
        }
        children.push_back(visit(choice.getCondition()));
        children.push_back(visitTupleOperation(choice));
        std::vector<size_t> data;
        data.push_back((encodeView(&choice)));
        return std::make_unique<InterpreterNode>(
                I_IndexChoice, &choice, std::move(children), nullptr, std::move(data));
    }

    NodePtr visitParallelIndexChoice(const RamParallelIndexChoice& ichoice) override {
        size_t relId = encodeRelation(ichoice.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        for (const auto& value : ichoice.getRangePattern().first) {
            children.push_back(visit(value));
        }
        for (const auto& value : ichoice.getRangePattern().second) {
            children.push_back(visit(value));
        }
        children.push_back(visit(ichoice.getCondition()));
        children.push_back(visit(ichoice.getOperation()));
        std::vector<size_t> data;
        data.push_back((encodeIndexPos(ichoice)));
        auto res = std::make_unique<InterpreterNode>(
                I_ParallelIndexChoice, &ichoice, std::move(children), rel, std::move(data));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitUnpackRecord(const RamUnpackRecord& lookup) override {  // get reference
        NodePtrVec children;
        children.push_back(visit(lookup.getExpression()));
        children.push_back(visitTupleOperation(lookup));
        return std::make_unique<InterpreterNode>(I_UnpackRecord, &lookup, std::move(children));
    }

    NodePtr visitAggregate(const RamAggregate& aggregate) override {
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visit(aggregate.getCondition()));
        children.push_back(visit(aggregate.getExpression()));
        children.push_back(visitTupleOperation(aggregate));
        return std::make_unique<InterpreterNode>(I_Aggregate, &aggregate, std::move(children), rel);
    }

    NodePtr visitParallelAggregate(const RamParallelAggregate& aggregate) override {
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visit(aggregate.getCondition()));
        children.push_back(visit(aggregate.getExpression()));
        children.push_back(visitTupleOperation(aggregate));
        auto res =
                std::make_unique<InterpreterNode>(I_ParallelAggregate, &aggregate, std::move(children), rel);
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitIndexAggregate(const RamIndexAggregate& aggregate) override {
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        for (const auto& value : aggregate.getRangePattern().first) {
            children.push_back(visit(value));
        }
        for (const auto& value : aggregate.getRangePattern().second) {
            children.push_back(visit(value));
        }
        children.push_back(visit(aggregate.getCondition()));
        children.push_back(visit(aggregate.getExpression()));
        children.push_back(visitTupleOperation(aggregate));
        std::vector<size_t> data;
        data.push_back((encodeView(&aggregate)));
        return std::make_unique<InterpreterNode>(
                I_IndexAggregate, &aggregate, std::move(children), rel, std::move(data));
    }

    NodePtr visitParallelIndexAggregate(const RamParallelIndexAggregate& aggregate) override {
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        for (const auto& value : aggregate.getRangePattern().first) {
            children.push_back(visit(value));
        }
        for (const auto& value : aggregate.getRangePattern().second) {
            children.push_back(visit(value));
        }
        children.push_back(visit(aggregate.getCondition()));
        children.push_back(visit(aggregate.getExpression()));
        children.push_back(visitTupleOperation(aggregate));
        std::vector<size_t> data;
        data.push_back((encodeView(&aggregate)));
        auto res = std::make_unique<InterpreterNode>(
                I_ParallelIndexAggregate, &aggregate, std::move(children), rel, std::move(data));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitBreak(const RamBreak& breakOp) override {
        NodePtrVec children;
        children.push_back(visit(breakOp.getCondition()));
        children.push_back(visit(breakOp.getOperation()));
        return std::make_unique<InterpreterNode>(I_Break, &breakOp, std::move(children));
    }

    NodePtr visitFilter(const RamFilter& filter) override {
        NodePtrVec children;
        children.push_back(visit(filter.getCondition()));
        children.push_back(visit(filter.getOperation()));
        return std::make_unique<InterpreterNode>(I_Filter, &filter, std::move(children));
    }

    NodePtr visitProject(const RamProject& project) override {
        size_t relId = encodeRelation(project.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        for (const auto& value : project.getValues()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterNode>(I_Project, &project, std::move(children), rel);
    }

    // -- return from subroutine --
    NodePtr visitSubroutineReturn(const RamSubroutineReturn& ret) override {
        NodePtrVec children;
        for (const auto& value : ret.getValues()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterNode>(I_SubroutineReturn, &ret, std::move(children));
    }

    NodePtr visitSequence(const RamSequence& seq) override {
        NodePtrVec children;
        for (const auto& value : seq.getStatements()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterNode>(I_Sequence, &seq, std::move(children));
    }

    NodePtr visitParallel(const RamParallel& parallel) override {
        // Parallel statements are executed in sequence for now.
        NodePtrVec children;
        for (const auto& value : parallel.getStatements()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterNode>(I_Parallel, &parallel, std::move(children));
    }

    NodePtr visitLoop(const RamLoop& loop) override {
        NodePtrVec children;
        children.push_back(visit(loop.getBody()));
        return std::make_unique<InterpreterNode>(I_Loop, &loop, std::move(children));
    }

    NodePtr visitExit(const RamExit& exit) override {
        NodePtrVec children;
        children.push_back(visit(exit.getCondition()));
        return std::make_unique<InterpreterNode>(I_Exit, &exit, std::move(children));
    }

    NodePtr visitCall(const RamCall& call) override {
        // translate a subroutine name to an index
        // the index is used to identify the subroutine
        // in the interpreter. The index is stored in the
        // data array of the InterpreterNode as the first
        // entry.
        auto subs = program->getSubroutines();
        size_t i = distance(subs.begin(), subs.find(call.getName()));
        std::vector<size_t> data;
        data.push_back(i);
        return std::make_unique<InterpreterNode>(I_Call, &call, NodePtrVec{}, nullptr, std::move(data));
    }

    NodePtr visitLogRelationTimer(const RamLogRelationTimer& timer) override {
        size_t relId = encodeRelation(timer.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visit(timer.getStatement()));
        return std::make_unique<InterpreterNode>(I_LogRelationTimer, &timer, std::move(children), rel);
    }

    NodePtr visitLogTimer(const RamLogTimer& timer) override {
        NodePtrVec children;
        children.push_back(visit(timer.getStatement()));
        return std::make_unique<InterpreterNode>(I_LogTimer, &timer, std::move(children));
    }

    NodePtr visitDebugInfo(const RamDebugInfo& dbg) override {
        NodePtrVec children;
        children.push_back(visit(dbg.getStatement()));
        return std::make_unique<InterpreterNode>(I_DebugInfo, &dbg, std::move(children));
    }

    NodePtr visitClear(const RamClear& clear) override {
        size_t relId = encodeRelation(clear.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterNode>(I_Clear, &clear, NodePtrVec{}, rel);
    }

    NodePtr visitLogSize(const RamLogSize& size) override {
        size_t relId = encodeRelation(size.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterNode>(I_LogSize, &size, NodePtrVec{}, rel);
    }

    NodePtr visitIO(const RamIO& io) override {
        size_t relId = encodeRelation(io.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterNode>(I_IO, &io, NodePtrVec{}, rel);
    }

    NodePtr visitQuery(const RamQuery& query) override {
        std::shared_ptr<InterpreterPreamble> preamble = std::make_shared<InterpreterPreamble>();
        parentQueryPreamble = preamble;
        // split terms of conditions of outer-most filter operation
        // into terms that require a context and terms that
        // do not require a view
        const RamOperation* next = &query.getOperation();
        std::vector<const RamCondition*> freeOfView;
        if (const auto* filter = dynamic_cast<const RamFilter*>(&query.getOperation())) {
            next = &filter->getOperation();
            // Check terms of outer filter operation whether they can be pushed before
            // the view-generation for speed improvements
            auto conditions = toConjunctionList(&filter->getCondition());
            for (auto const& cur : conditions) {
                bool needView = false;
                visitDepthFirst(*cur, [&](const RamNode& node) {
                    if (requireView(&node)) {
                        needView = true;
                        const auto& rel = getRelationRefForView(&node);
                        preamble->addViewInfoForFilter(
                                encodeRelation(rel), indexTable[&node], encodeView(&node));
                    }
                });

                if (needView) {
                    preamble->addViewOperationForFilter(visit(*cur));
                } else {
                    preamble->addViewFreeOperationForFilter(visit(*cur));
                }
            }
        }

        visitDepthFirst(*next, [&](const RamNode& node) {
            if (requireView(&node)) {
                const auto& rel = getRelationRefForView(&node);
                preamble->addViewInfoForNested(encodeRelation(rel), indexTable[&node], encodeView(&node));
            };
        });

        visitDepthFirst(*next, [&](const RamAbstractParallel&) { preamble->isParallel = true; });

        NodePtrVec children;
        children.push_back(visit(*next));

        auto res = std::make_unique<InterpreterNode>(I_Query, &query, std::move(children));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitExtend(const RamExtend& extend) override {
        std::vector<size_t> data;
        data.push_back((encodeRelation(extend.getFirstRelation())));
        data.push_back(encodeRelation(extend.getSecondRelation()));
        return std::make_unique<InterpreterNode>(I_Extend, &extend, NodePtrVec{}, nullptr, std::move(data));
    }

    NodePtr visitSwap(const RamSwap& swap) override {
        std::vector<size_t> data;
        data.push_back((encodeRelation(swap.getFirstRelation())));
        data.push_back((encodeRelation(swap.getSecondRelation())));
        return std::make_unique<InterpreterNode>(I_Swap, &swap, NodePtrVec{}, nullptr, std::move(data));
    }

    NodePtr visitUndefValue(const RamUndefValue&) override {
        return nullptr;
    }

    NodePtr visitNode(const RamNode& node) override {
        fatal("unsupported node type: %s", typeid(node).name());
    }

public:
    /** @brief Move relation map */
    std::vector<std::unique_ptr<RelationHandle>>& getRelations() {
        return relations;
    }

    /** @brief Return relation handle from given index */
    RelationHandle& getRelationHandle(const size_t idx) {
        return *relations[idx];
    }

private:
    /** Environment encoding, store a mapping from RamNode to its operation index id. */
    std::unordered_map<const RamNode*, size_t> indexTable;
    /** Used by index encoding */
    RamIndexAnalysis* isa;
    /** Points to the current preamble during the generation.  It is used to passing preamble between parent
     * query and its nested parallel operation. */
    std::shared_ptr<InterpreterPreamble> parentQueryPreamble = nullptr;
    /** Next available location to encode View */
    size_t viewId = 0;
    /** Next available location to encode a relation */
    size_t relId = 0;
    /** Environment encoding, store a mapping from RamNode to its View id. */
    std::unordered_map<const RamNode*, size_t> viewTable;
    /** Environment encoding, store a mapping from RamRelation to its id */
    std::unordered_map<const RamRelation*, size_t> relTable;
    /** Symbol table for relations */
    std::vector<std::unique_ptr<RelationHandle>> relations;
    /** If generating a provenance program */
    const bool isProvenance;
    /** RamProgram */
    RamProgram* program;

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
        const MinIndexSelection& orderSet = isa->getIndexes(node.getRelation());
        SearchSignature signature = isa->getSearchSignature(&node);
        // A zero signature is equivalent as a full order signature.
        if (signature.empty()) {
            signature = SearchSignature::getFullSearchSignature(signature.arity());
        }
        auto i = orderSet.getLexOrderNum(signature);
        indexTable[&node] = i;
        return i;
    };

    /** @brief Encode and return the View id of an operation. */
    size_t encodeView(const RamNode* node) {
        auto pos = viewTable.find(node);
        if (pos != viewTable.end()) {
            return pos->second;
        }
        size_t id = getNextViewId();
        viewTable[node] = id;
        return id;
    }

    /** @brief Encode and create the relation, return the relation id */
    size_t encodeRelation(const RamRelation& rel) {
        auto pos = relTable.find(&rel);
        if (pos != relTable.end()) {
            return pos->second;
        }
        size_t id = getNewRelId();
        relTable[&rel] = id;
        createRelation(rel, isa->getIndexes(rel), id);
        return id;
    }

    /**
     * @brief Find all operations under the root node that requires a view.
     * Return a list of InterpreterNodes.
     */
    NodePtrVec findAllViews(const RamNode& node) {
        NodePtrVec res;
        visitDepthFirst(node, [&](const RamNode& node) {
            if (requireView(&node)) {
                res.push_back(visit(node));
            };
        });
        return res;
    }

    /**
     * Return true if the given operation requires a view.
     */
    bool requireView(const RamNode* node) {
        if (dynamic_cast<const RamAbstractExistenceCheck*>(node) != nullptr) {
            return true;
        } else if (dynamic_cast<const RamIndexOperation*>(node) != nullptr) {
            return true;
        }
        return false;
    }

    /**
     * @brief Return the associated relation of a operation which requires a view.
     * This function assume the operation does requires a view.
     */
    const RamRelation& getRelationRefForView(const RamNode* node) {
        if (const auto* exist = dynamic_cast<const RamAbstractExistenceCheck*>(node)) {
            return exist->getRelation();
        } else if (const auto* index = dynamic_cast<const RamIndexOperation*>(node)) {
            return index->getRelation();
        }

        fatal("The RamNode does not require a view.");
    }

    /**
     * @brief Convert terms of a conjunction to a list
     *
     * Convert a condition of the format C1 /\ C2 /\ ... /\ Cn
     * to a list {C1, C2, ..., Cn}.
     */
    inline std::vector<const RamCondition*> toConjunctionList(const RamCondition* condition) {
        std::vector<const RamCondition*> conditionList;
        std::queue<const RamCondition*> conditionsToProcess;
        if (condition != nullptr) {
            conditionsToProcess.push(condition);
            while (!conditionsToProcess.empty()) {
                condition = conditionsToProcess.front();
                conditionsToProcess.pop();
                if (const auto* ramConj = dynamic_cast<const RamConjunction*>(condition)) {
                    conditionsToProcess.push(&ramConj->getLHS());
                    conditionsToProcess.push(&ramConj->getRHS());
                } else {
                    conditionList.emplace_back(condition);
                }
            }
        }
        return conditionList;
    }

    void createRelation(const RamRelation& id, const MinIndexSelection& orderSet, const size_t idx) {
        RelationHandle res;
        if (relations.size() < idx + 1) {
            relations.resize(idx + 1);
        }
        if (id.getRepresentation() == RelationRepresentation::EQREL) {
            res = std::make_unique<InterpreterEqRelation>(id.getArity(), id.getAuxiliaryArity(), id.getName(),
                    std::vector<std::string>(), orderSet);
        } else {
            if (isProvenance) {
                res = std::make_unique<InterpreterRelation>(id.getArity(), id.getAuxiliaryArity(),
                        id.getName(), std::vector<std::string>(), orderSet, createBTreeProvenanceIndex);
            } else {
                res = std::make_unique<InterpreterRelation>(id.getArity(), id.getAuxiliaryArity(),
                        id.getName(), std::vector<std::string>(), orderSet);
            }
        }
        relations[idx] = std::make_unique<RelationHandle>(std::move(res));
    }
};
}  // namespace souffle
