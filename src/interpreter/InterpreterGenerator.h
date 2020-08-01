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
#include "RamTypes.h"
#include "RelationTag.h"
#include "interpreter/InterpreterIndex.h"
#include "interpreter/InterpreterNode.h"
#include "interpreter/InterpreterPreamble.h"
#include "interpreter/InterpreterRelation.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/Utils.h"
#include "ram/Visitor.h"
#include "ram/analysis/IndexAnalysis.h"
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
    NodeGenerator(RamIndexAnalysis* isa)
            : isa(isa), isProvenance(Global::config().has("provenance")),
              profileEnabled(Global::config().has("profile")) {}

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
        return std::make_unique<InterpreterConstant>(I_Constant, &num);
    }

    NodePtr visitTupleElement(const RamTupleElement& access) override {
        return std::make_unique<InterpreterTupleElement>(I_TupleElement, &access);
    }

    NodePtr visitAutoIncrement(const RamAutoIncrement& inc) override {
        return std::make_unique<InterpreterAutoIncrement>(I_AutoIncrement, &inc);
    }

    NodePtr visitIntrinsicOperator(const RamIntrinsicOperator& op) override {
        NodePtrVec children;
        for (const auto& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        return std::make_unique<InterpreterIntrinsicOperator>(I_IntrinsicOperator, &op, std::move(children));
    }

    NodePtr visitUserDefinedOperator(const RamUserDefinedOperator& op) override {
        NodePtrVec children;
        for (const auto& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        return std::make_unique<InterpreterUserDefinedOperator>(
                I_UserDefinedOperator, &op, std::move(children));
    }

    NodePtr visitNestedIntrinsicOperator(const RamNestedIntrinsicOperator& op) override {
        NodePtrVec children;
        for (auto&& arg : op.getArguments()) {
            children.push_back(visit(arg));
        }
        children.push_back(visitTupleOperation(op));
        return std::make_unique<InterpreterNestedIntrinsicOperator>(
                I_NestedIntrinsicOperator, &op, std::move(children));
    }

    NodePtr visitPackRecord(const RamPackRecord& pr) override {
        NodePtrVec children;
        for (const auto& arg : pr.getArguments()) {
            children.push_back(visit(arg));
        }
        return std::make_unique<InterpreterPackRecord>(I_PackRecord, &pr, std::move(children));
    }

    NodePtr visitSubroutineArgument(const RamSubroutineArgument& arg) override {
        return std::make_unique<InterpreterSubroutineArgument>(I_SubroutineArgument, &arg);
    }

    // -- connectors operators --
    NodePtr visitTrue(const RamTrue& ltrue) override {
        return std::make_unique<InterpreterTrue>(I_True, &ltrue);
    }

    NodePtr visitFalse(const RamFalse& lfalse) override {
        return std::make_unique<InterpreterFalse>(I_False, &lfalse);
    }

    NodePtr visitConjunction(const RamConjunction& conj) override {
        return std::make_unique<InterpreterConjunction>(
                I_Conjunction, &conj, visit(conj.getLHS()), visit(conj.getRHS()));
    }

    NodePtr visitNegation(const RamNegation& neg) override {
        return std::make_unique<InterpreterNegation>(I_Negation, &neg, visit(neg.getOperand()));
    }

    NodePtr visitEmptinessCheck(const RamEmptinessCheck& emptiness) override {
        size_t relId = encodeRelation(emptiness.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterEmptinessCheck>(I_EmptinessCheck, &emptiness, rel);
    }

    NodePtr visitExistenceCheck(const RamExistenceCheck& exists) override {
        InterpreterSuperInstruction superOp = getExistenceSuperInstInfo(exists);
        // Check if the search signature is a total signature
        bool isTotal = true;
        for (const auto& cur : exists.getValues()) {
            if (isRamUndefValue(cur)) {
                isTotal = false;
            }
        }
        return std::make_unique<InterpreterExistenceCheck>(
                I_ExistenceCheck, &exists, isTotal, encodeView(&exists), std::move(superOp));
    }

    NodePtr visitProvenanceExistenceCheck(const RamProvenanceExistenceCheck& provExists) override {
        InterpreterSuperInstruction superOp = getExistenceSuperInstInfo(provExists);
        return std::make_unique<InterpreterProvenanceExistenceCheck>(I_ProvenanceExistenceCheck, &provExists,
                visit(provExists.getChildNodes().back()), encodeView(&provExists), std::move(superOp));
    }

    // -- comparison operators --
    NodePtr visitConstraint(const RamConstraint& relOp) override {
        return std::make_unique<InterpreterConstraint>(
                I_Constraint, &relOp, visit(relOp.getLHS()), visit(relOp.getRHS()));
    }

    NodePtr visitNestedOperation(const RamNestedOperation& nested) override {
        return visit(nested.getOperation());
    }

    NodePtr visitTupleOperation(const RamTupleOperation& search) override {
        if (profileEnabled) {
            return std::make_unique<InterpreterTupleOperation>(
                    I_TupleOperation, &search, visit(search.getOperation()));
        }
        return visit(search.getOperation());
    }

    NodePtr visitScan(const RamScan& scan) override {
        size_t relId = encodeRelation(scan.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterScan>(I_Scan, &scan, rel, visitTupleOperation(scan));
    }

    NodePtr visitParallelScan(const RamParallelScan& pScan) override {
        size_t relId = encodeRelation(pScan.getRelation());
        auto rel = relations[relId].get();
        auto res = std::make_unique<InterpreterParallelScan>(
                I_ParallelScan, &pScan, rel, visitTupleOperation(pScan));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitIndexScan(const RamIndexScan& scan) override {
        InterpreterSuperInstruction indexOperation = getIndexSuperInstInfo(scan);
        NodePtrVec children;
        children.push_back(visitTupleOperation(scan));
        return std::make_unique<InterpreterIndexScan>(I_IndexScan, &scan, nullptr, visitTupleOperation(scan),
                encodeView(&scan), std::move(indexOperation));
    }

    NodePtr visitParallelIndexScan(const RamParallelIndexScan& piscan) override {
        size_t relId = encodeRelation(piscan.getRelation());
        auto rel = relations[relId].get();
        InterpreterSuperInstruction indexOperation = getIndexSuperInstInfo(piscan);
        auto res = std::make_unique<InterpreterParallelIndexScan>(I_ParallelIndexScan, &piscan, rel,
                visitTupleOperation(piscan), encodeIndexPos(piscan), std::move(indexOperation));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitChoice(const RamChoice& choice) override {
        size_t relId = encodeRelation(choice.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterChoice>(
                I_Choice, &choice, rel, visit(choice.getCondition()), visitTupleOperation(choice));
    }

    NodePtr visitParallelChoice(const RamParallelChoice& pchoice) override {
        size_t relId = encodeRelation(pchoice.getRelation());
        auto rel = relations[relId].get();
        auto res = std::make_unique<InterpreterParallelChoice>(
                I_ParallelChoice, &pchoice, rel, visit(pchoice.getCondition()), visitTupleOperation(pchoice));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitIndexChoice(const RamIndexChoice& choice) override {
        InterpreterSuperInstruction indexOperation = getIndexSuperInstInfo(choice);
        return std::make_unique<InterpreterIndexChoice>(I_IndexChoice, &choice, nullptr,
                visit(choice.getCondition()), visitTupleOperation(choice), encodeView(&choice),
                std::move(indexOperation));
    }

    NodePtr visitParallelIndexChoice(const RamParallelIndexChoice& ichoice) override {
        InterpreterSuperInstruction indexOperation = getIndexSuperInstInfo(ichoice);
        size_t relId = encodeRelation(ichoice.getRelation());
        auto rel = relations[relId].get();
        auto res = std::make_unique<InterpreterParallelIndexChoice>(I_ParallelIndexChoice, &ichoice, rel,
                visit(ichoice.getCondition()), visit(ichoice.getOperation()), encodeIndexPos(ichoice),
                std::move(indexOperation));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitUnpackRecord(const RamUnpackRecord& lookup) override {  // get reference
        return std::make_unique<InterpreterUnpackRecord>(
                I_UnpackRecord, &lookup, visit(lookup.getExpression()), visitTupleOperation(lookup));
    }

    NodePtr visitAggregate(const RamAggregate& aggregate) override {
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterAggregate>(I_Aggregate, &aggregate, rel,
                visit(aggregate.getExpression()), visit(aggregate.getCondition()),
                visitTupleOperation(aggregate));
    }

    NodePtr visitParallelAggregate(const RamParallelAggregate& aggregate) override {
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        auto res = std::make_unique<InterpreterParallelAggregate>(I_ParallelAggregate, &aggregate, rel,
                visit(aggregate.getExpression()), visit(aggregate.getCondition()),
                visitTupleOperation(aggregate));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitIndexAggregate(const RamIndexAggregate& aggregate) override {
        InterpreterSuperInstruction indexOperation = getIndexSuperInstInfo(aggregate);
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterIndexAggregate>(I_IndexAggregate, &aggregate, rel,
                visit(aggregate.getExpression()), visit(aggregate.getCondition()),
                visitTupleOperation(aggregate), encodeView(&aggregate), std::move(indexOperation));
    }

    NodePtr visitParallelIndexAggregate(const RamParallelIndexAggregate& aggregate) override {
        InterpreterSuperInstruction indexOperation = getIndexSuperInstInfo(aggregate);
        size_t relId = encodeRelation(aggregate.getRelation());
        auto rel = relations[relId].get();
        auto res = std::make_unique<InterpreterParallelIndexAggregate>(I_ParallelIndexAggregate, &aggregate,
                rel, visit(aggregate.getExpression()), visit(aggregate.getCondition()),
                visitTupleOperation(aggregate), encodeView(&aggregate), std::move(indexOperation));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitBreak(const RamBreak& breakOp) override {
        return std::make_unique<InterpreterBreak>(
                I_Break, &breakOp, visit(breakOp.getCondition()), visit(breakOp.getOperation()));
    }

    NodePtr visitFilter(const RamFilter& filter) override {
        return std::make_unique<InterpreterFilter>(
                I_Filter, &filter, visit(filter.getCondition()), visit(filter.getOperation()));
    }

    NodePtr visitProject(const RamProject& project) override {
        InterpreterSuperInstruction superOp = getProjectSuperInstInfo(project);
        size_t relId = encodeRelation(project.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterProject>(I_Project, &project, rel, std::move(superOp));
    }

    // -- return from subroutine --
    NodePtr visitSubroutineReturn(const RamSubroutineReturn& ret) override {
        NodePtrVec children;
        for (const auto& value : ret.getValues()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterSubroutineReturn>(I_SubroutineReturn, &ret, std::move(children));
    }

    NodePtr visitSequence(const RamSequence& seq) override {
        NodePtrVec children;
        for (const auto& value : seq.getStatements()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterSequence>(I_Sequence, &seq, std::move(children));
    }

    NodePtr visitParallel(const RamParallel& parallel) override {
        // Parallel statements are executed in sequence for now.
        NodePtrVec children;
        for (const auto& value : parallel.getStatements()) {
            children.push_back(visit(value));
        }
        return std::make_unique<InterpreterParallel>(I_Parallel, &parallel, std::move(children));
    }

    NodePtr visitLoop(const RamLoop& loop) override {
        return std::make_unique<InterpreterLoop>(I_Loop, &loop, visit(loop.getBody()));
    }

    NodePtr visitExit(const RamExit& exit) override {
        return std::make_unique<InterpreterExit>(I_Exit, &exit, visit(exit.getCondition()));
    }

    NodePtr visitCall(const RamCall& call) override {
        // translate a subroutine name to an index
        // the index is used to identify the subroutine
        // in the interpreter. The index is stored in the
        // data array of the InterpreterNode as the first
        // entry.
        auto subs = program->getSubroutines();
        size_t subroutineId = distance(subs.begin(), subs.find(call.getName()));
        return std::make_unique<InterpreterCall>(I_Call, &call, subroutineId);
    }

    NodePtr visitLogRelationTimer(const RamLogRelationTimer& timer) override {
        size_t relId = encodeRelation(timer.getRelation());
        auto rel = relations[relId].get();
        NodePtrVec children;
        children.push_back(visit(timer.getStatement()));
        return std::make_unique<InterpreterLogRelationTimer>(
                I_LogRelationTimer, &timer, visit(timer.getStatement()), rel);
    }

    NodePtr visitLogTimer(const RamLogTimer& timer) override {
        NodePtrVec children;
        children.push_back(visit(timer.getStatement()));
        return std::make_unique<InterpreterLogTimer>(I_LogTimer, &timer, visit(timer.getStatement()));
    }

    NodePtr visitDebugInfo(const RamDebugInfo& dbg) override {
        NodePtrVec children;
        children.push_back(visit(dbg.getStatement()));
        return std::make_unique<InterpreterDebugInfo>(I_DebugInfo, &dbg, visit(dbg.getStatement()));
    }

    NodePtr visitClear(const RamClear& clear) override {
        size_t relId = encodeRelation(clear.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterClear>(I_Clear, &clear, rel);
    }

    NodePtr visitLogSize(const RamLogSize& size) override {
        size_t relId = encodeRelation(size.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterLogSize>(I_LogSize, &size, rel);
    }

    NodePtr visitIO(const RamIO& io) override {
        size_t relId = encodeRelation(io.getRelation());
        auto rel = relations[relId].get();
        return std::make_unique<InterpreterIO>(I_IO, &io, rel);
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

        auto res = std::make_unique<InterpreterQuery>(I_Query, &query, visit(*next));
        res->setPreamble(parentQueryPreamble);
        return res;
    }

    NodePtr visitExtend(const RamExtend& extend) override {
        size_t src = encodeRelation(extend.getFirstRelation());
        size_t target = encodeRelation(extend.getSecondRelation());
        return std::make_unique<InterpreterExtend>(I_Extend, &extend, src, target);
    }

    NodePtr visitSwap(const RamSwap& swap) override {
        size_t src = encodeRelation(swap.getFirstRelation());
        size_t target = encodeRelation(swap.getSecondRelation());
        return std::make_unique<InterpreterSwap>(I_Swap, &swap, src, target);
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
    /** If profile is enable in this program */
    const bool profileEnabled;
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

    /**
     * @brief Create and add relation into the runtime environment.
     */
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

    /**
     * @brief Encode and return the super-instruction information about a index operation.
     */
    InterpreterSuperInstruction getIndexSuperInstInfo(const RamIndexOperation& ramIndex) {
        size_t arity = ramIndex.getRelation().getArity();
        InterpreterSuperInstruction indexOperation(arity);
        const auto& first = ramIndex.getRangePattern().first;
        for (size_t i = 0; i < arity; ++i) {
            auto& low = first[i];
            // Unbounded
            if (isRamUndefValue(low)) {
                indexOperation.first[i] = MIN_RAM_SIGNED;
                continue;
            }

            // Constant
            if (dynamic_cast<RamConstant*>(low) != nullptr) {
                indexOperation.first[i] = dynamic_cast<RamConstant*>(low)->getConstant();
                continue;
            }

            // TupleElement
            if (dynamic_cast<RamTupleElement*>(low) != nullptr) {
                auto lowTuple = dynamic_cast<RamTupleElement*>(low);
                indexOperation.tupleFirst.push_back(
                        {i, (size_t)lowTuple->getTupleId(), lowTuple->getElement()});
                continue;
            }

            // Generic expression
            indexOperation.exprFirst.push_back(
                    std::pair<size_t, std::unique_ptr<InterpreterNode>>(i, visit(low)));
        }
        const auto& second = ramIndex.getRangePattern().second;
        for (size_t i = 0; i < arity; ++i) {
            auto& hig = second[i];
            // Unbounded
            if (isRamUndefValue(hig)) {
                indexOperation.second[i] = MAX_RAM_SIGNED;
                continue;
            }

            // Constant
            if (dynamic_cast<RamConstant*>(hig) != nullptr) {
                indexOperation.second[i] = dynamic_cast<RamConstant*>(hig)->getConstant();
                continue;
            }

            // TupleElement
            if (dynamic_cast<RamTupleElement*>(hig) != nullptr) {
                auto highTuple = dynamic_cast<RamTupleElement*>(hig);
                indexOperation.tupleSecond.push_back(
                        {i, (size_t)highTuple->getTupleId(), highTuple->getElement()});
                continue;
            }

            // Generic expression
            indexOperation.exprSecond.push_back(
                    std::pair<size_t, std::unique_ptr<InterpreterNode>>(i, visit(hig)));
        }
        return indexOperation;
    }

    /**
     * @brief Encode and return the super-instruction information about an existence check operation
     */
    InterpreterSuperInstruction getExistenceSuperInstInfo(const RamAbstractExistenceCheck& exist) {
        size_t arity = exist.getRelation().getArity();
        InterpreterSuperInstruction superOp(arity);
        const auto& children = exist.getValues();
        for (size_t i = 0; i < arity; ++i) {
            auto& child = children[i];
            // Unbounded
            if (isRamUndefValue(child)) {
                superOp.first[i] = MIN_RAM_SIGNED;
                superOp.second[i] = MAX_RAM_SIGNED;
                continue;
            }

            // Constant
            if (dynamic_cast<RamConstant*>(child) != nullptr) {
                superOp.first[i] = dynamic_cast<RamConstant*>(child)->getConstant();
                superOp.second[i] = superOp.first[i];
                continue;
            }

            // TupleElement
            if (dynamic_cast<RamTupleElement*>(child) != nullptr) {
                auto tuple = dynamic_cast<RamTupleElement*>(child);
                superOp.tupleFirst.push_back({i, (size_t)tuple->getTupleId(), tuple->getElement()});
                continue;
            }

            // Generic expression
            superOp.exprFirst.push_back(std::pair<size_t, std::unique_ptr<InterpreterNode>>(i, visit(child)));
        }
        return superOp;
    }

    /**
     * @brief Encode and return the super-instruction information about a project operation
     */
    InterpreterSuperInstruction getProjectSuperInstInfo(const RamProject& exist) {
        size_t arity = exist.getRelation().getArity();
        InterpreterSuperInstruction superOp(arity);
        const auto& children = exist.getValues();
        for (size_t i = 0; i < arity; ++i) {
            auto& child = children[i];
            // Constant
            if (dynamic_cast<RamConstant*>(child) != nullptr) {
                superOp.first[i] = dynamic_cast<RamConstant*>(child)->getConstant();
                continue;
            }

            // TupleElement
            if (dynamic_cast<RamTupleElement*>(child) != nullptr) {
                auto tuple = dynamic_cast<RamTupleElement*>(child);
                superOp.tupleFirst.push_back({i, (size_t)tuple->getTupleId(), tuple->getElement()});
                continue;
            }

            // Generic expression
            superOp.exprFirst.push_back(std::pair<size_t, std::unique_ptr<InterpreterNode>>(i, visit(child)));
        }
        return superOp;
    }
};
}  // namespace souffle
