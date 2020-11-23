/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Generator.cpp
 *
 * Define the Interpreter Generator class.
 ***********************************************************************/

#include "interpreter/Generator.h"
#include "interpreter/Engine.h"

namespace souffle::interpreter {

using NodePtr = Own<Node>;
using NodePtrVec = std::vector<NodePtr>;
using RelationHandle = Own<RelationWrapper>;

NodeGenerator::NodeGenerator(Engine& engine) : engine(engine) {
    visitDepthFirst(engine.tUnit.getProgram(), [&](const ram::Relation& relation) {
        assert(relationMap.find(relation.getName()) == relationMap.end() && "double-naming of relations");
        relationMap[relation.getName()] = &relation;
    });
}

NodePtr NodeGenerator::generateTree(const ram::Node& root) {
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

NodePtr NodeGenerator::visitConstant(const ram::Constant& num) {
    return mk<Constant>(I_Constant, &num);
}

NodePtr NodeGenerator::visitTupleElement(const ram::TupleElement& access) {
    auto tupleId = access.getTupleId();
    auto elementId = access.getElement();
    auto newElementId = orderingContext.mapOrder(tupleId, elementId);
    return mk<TupleElement>(I_TupleElement, &access, tupleId, newElementId);
}

NodePtr NodeGenerator::visitAutoIncrement(const ram::AutoIncrement& inc) {
    return mk<AutoIncrement>(I_AutoIncrement, &inc);
}

NodePtr NodeGenerator::visitIntrinsicOperator(const ram::IntrinsicOperator& op) {
    NodePtrVec children;
    for (const auto& arg : op.getArguments()) {
        children.push_back(visit(arg));
    }
    return mk<IntrinsicOperator>(I_IntrinsicOperator, &op, std::move(children));
}

NodePtr NodeGenerator::visitUserDefinedOperator(const ram::UserDefinedOperator& op) {
    NodePtrVec children;
    for (const auto& arg : op.getArguments()) {
        children.push_back(visit(arg));
    }
    return mk<UserDefinedOperator>(I_UserDefinedOperator, &op, std::move(children));
}

NodePtr NodeGenerator::visitNestedIntrinsicOperator(const ram::NestedIntrinsicOperator& op) {
    auto arity = op.getArguments().size();
    orderingContext.addNewTuple(op.getTupleId(), arity);
    NodePtrVec children;
    for (auto&& arg : op.getArguments()) {
        children.push_back(visit(arg));
    }
    children.push_back(visitTupleOperation(op));
    return mk<NestedIntrinsicOperator>(I_NestedIntrinsicOperator, &op, std::move(children));
}

NodePtr NodeGenerator::visitPackRecord(const ram::PackRecord& pr) {
    NodePtrVec children;
    for (const auto& arg : pr.getArguments()) {
        children.push_back(visit(arg));
    }
    return mk<PackRecord>(I_PackRecord, &pr, std::move(children));
}

NodePtr NodeGenerator::visitSubroutineArgument(const ram::SubroutineArgument& arg) {
    return mk<SubroutineArgument>(I_SubroutineArgument, &arg);
}

// -- connectors operators --
NodePtr NodeGenerator::visitTrue(const ram::True& ltrue) {
    return mk<True>(I_True, &ltrue);
}

NodePtr NodeGenerator::visitFalse(const ram::False& lfalse) {
    return mk<False>(I_False, &lfalse);
}

NodePtr NodeGenerator::visitConjunction(const ram::Conjunction& conj) {
    return mk<Conjunction>(I_Conjunction, &conj, visit(conj.getLHS()), visit(conj.getRHS()));
}

NodePtr NodeGenerator::visitNegation(const ram::Negation& neg) {
    return mk<Negation>(I_Negation, &neg, visit(neg.getOperand()));
}

NodePtr NodeGenerator::visitEmptinessCheck(const ram::EmptinessCheck& emptiness) {
    size_t relId = encodeRelation(emptiness.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("EmptinessCheck", lookup(emptiness.getRelation()));
    return mk<EmptinessCheck>(type, &emptiness, rel);
}

NodePtr NodeGenerator::visitRelationSize(const ram::RelationSize& size) {
    size_t relId = encodeRelation(size.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("RelationSize", lookup(size.getRelation()));
    return mk<RelationSize>(type, &size, rel);
}

NodePtr NodeGenerator::visitExistenceCheck(const ram::ExistenceCheck& exists) {
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

NodePtr NodeGenerator::visitProvenanceExistenceCheck(const ram::ProvenanceExistenceCheck& provExists) {
    SuperInstruction superOp = getExistenceSuperInstInfo(provExists);
    NodeType type = constructNodeType("ProvenanceExistenceCheck", lookup(provExists.getRelation()));
    return mk<ProvenanceExistenceCheck>(type, &provExists, visit(provExists.getChildNodes().back()),
            encodeView(&provExists), std::move(superOp));
}

NodePtr NodeGenerator::visitConstraint(const ram::Constraint& relOp) {
    return mk<Constraint>(I_Constraint, &relOp, visit(relOp.getLHS()), visit(relOp.getRHS()));
}

NodePtr NodeGenerator::visitNestedOperation(const ram::NestedOperation& nested) {
    return visit(nested.getOperation());
}

NodePtr NodeGenerator::visitTupleOperation(const ram::TupleOperation& search) {
    return visit(search.getOperation());
}

NodePtr NodeGenerator::visitScan(const ram::Scan& scan) {
    orderingContext.addTupleWithDefaultOrder(scan.getTupleId(), scan);
    size_t relId = encodeRelation(scan.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("Scan", lookup(scan.getRelation()));
    return mk<Scan>(type, &scan, rel, visitTupleOperation(scan));
}

NodePtr NodeGenerator::visitParallelScan(const ram::ParallelScan& pScan) {
    orderingContext.addTupleWithDefaultOrder(pScan.getTupleId(), pScan);
    size_t relId = encodeRelation(pScan.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("ParallelScan", lookup(pScan.getRelation()));
    auto res = mk<ParallelScan>(type, &pScan, rel, visitTupleOperation(pScan));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visitIndexScan(const ram::IndexScan& iScan) {
    orderingContext.addTupleWithIndexOrder(iScan.getTupleId(), iScan);
    SuperInstruction indexOperation = getIndexSuperInstInfo(iScan);
    NodeType type = constructNodeType("IndexScan", lookup(iScan.getRelation()));
    return mk<IndexScan>(
            type, &iScan, nullptr, visitTupleOperation(iScan), encodeView(&iScan), std::move(indexOperation));
}

NodePtr NodeGenerator::visitParallelIndexScan(const ram::ParallelIndexScan& piscan) {
    orderingContext.addTupleWithIndexOrder(piscan.getTupleId(), piscan);
    SuperInstruction indexOperation = getIndexSuperInstInfo(piscan);
    size_t relId = encodeRelation(piscan.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("ParallelIndexScan", lookup(piscan.getRelation()));
    auto res = mk<ParallelIndexScan>(type, &piscan, rel, visitTupleOperation(piscan), encodeIndexPos(piscan),
            std::move(indexOperation));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visitChoice(const ram::Choice& choice) {
    orderingContext.addTupleWithDefaultOrder(choice.getTupleId(), choice);
    size_t relId = encodeRelation(choice.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("Choice", lookup(choice.getRelation()));
    return mk<Choice>(type, &choice, rel, visit(choice.getCondition()), visitTupleOperation(choice));
}

NodePtr NodeGenerator::visitParallelChoice(const ram::ParallelChoice& pChoice) {
    orderingContext.addTupleWithDefaultOrder(pChoice.getTupleId(), pChoice);
    size_t relId = encodeRelation(pChoice.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("ParallelChoice", lookup(pChoice.getRelation()));
    auto res = mk<ParallelChoice>(
            type, &pChoice, rel, visit(pChoice.getCondition()), visitTupleOperation(pChoice));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visitIndexChoice(const ram::IndexChoice& iChoice) {
    orderingContext.addTupleWithIndexOrder(iChoice.getTupleId(), iChoice);
    SuperInstruction indexOperation = getIndexSuperInstInfo(iChoice);
    NodeType type = constructNodeType("IndexChoice", lookup(iChoice.getRelation()));
    return mk<IndexChoice>(type, &iChoice, nullptr, visit(iChoice.getCondition()),
            visitTupleOperation(iChoice), encodeView(&iChoice), std::move(indexOperation));
}

NodePtr NodeGenerator::visitParallelIndexChoice(const ram::ParallelIndexChoice& piChoice) {
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

NodePtr NodeGenerator::visitUnpackRecord(const ram::UnpackRecord& unpack) {  // get reference
    orderingContext.addNewTuple(unpack.getTupleId(), unpack.getArity());
    return mk<UnpackRecord>(
            I_UnpackRecord, &unpack, visit(unpack.getExpression()), visitTupleOperation(unpack));
}

NodePtr NodeGenerator::visitAggregate(const ram::Aggregate& aggregate) {
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

NodePtr NodeGenerator::visitParallelAggregate(const ram::ParallelAggregate& pAggregate) {
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

NodePtr NodeGenerator::visitIndexAggregate(const ram::IndexAggregate& iAggregate) {
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

NodePtr NodeGenerator::visitParallelIndexAggregate(const ram::ParallelIndexAggregate& piAggregate) {
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

NodePtr NodeGenerator::visitBreak(const ram::Break& breakOp) {
    return mk<Break>(I_Break, &breakOp, visit(breakOp.getCondition()), visit(breakOp.getOperation()));
}

NodePtr NodeGenerator::visitFilter(const ram::Filter& filter) {
    return mk<Filter>(I_Filter, &filter, visit(filter.getCondition()), visit(filter.getOperation()));
}

NodePtr NodeGenerator::visitProject(const ram::Project& project) {
    SuperInstruction superOp = getProjectSuperInstInfo(project);
    size_t relId = encodeRelation(project.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("Project", lookup(project.getRelation()));
    return mk<Project>(type, &project, rel, std::move(superOp));
}

NodePtr NodeGenerator::visitSubroutineReturn(const ram::SubroutineReturn& ret) {
    NodePtrVec children;
    for (const auto& value : ret.getValues()) {
        children.push_back(visit(value));
    }
    return mk<SubroutineReturn>(I_SubroutineReturn, &ret, std::move(children));
}

NodePtr NodeGenerator::visitSequence(const ram::Sequence& seq) {
    NodePtrVec children;
    for (const auto& value : seq.getStatements()) {
        children.push_back(visit(value));
    }
    return mk<Sequence>(I_Sequence, &seq, std::move(children));
}

NodePtr NodeGenerator::visitParallel(const ram::Parallel& parallel) {
    // Parallel statements are executed in sequence for now.
    NodePtrVec children;
    for (const auto& value : parallel.getStatements()) {
        children.push_back(visit(value));
    }
    return mk<Parallel>(I_Parallel, &parallel, std::move(children));
}

NodePtr NodeGenerator::visitLoop(const ram::Loop& loop) {
    return mk<Loop>(I_Loop, &loop, visit(loop.getBody()));
}

NodePtr NodeGenerator::visitExit(const ram::Exit& exit) {
    return mk<Exit>(I_Exit, &exit, visit(exit.getCondition()));
}

NodePtr NodeGenerator::visitCall(const ram::Call& call) {
    // translate a subroutine name to an index
    // the index is used to identify the subroutine
    // in the interpreter. The index is stored in the
    // data array of the Node as the first
    // entry.
    auto subs = engine.tUnit.getProgram().getSubroutines();
    size_t subroutineId = distance(subs.begin(), subs.find(call.getName()));
    return mk<Call>(I_Call, &call, subroutineId);
}

NodePtr NodeGenerator::visitLogRelationTimer(const ram::LogRelationTimer& timer) {
    size_t relId = encodeRelation(timer.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<LogRelationTimer>(I_LogRelationTimer, &timer, visit(timer.getStatement()), rel);
}

NodePtr NodeGenerator::visitLogTimer(const ram::LogTimer& timer) {
    return mk<LogTimer>(I_LogTimer, &timer, visit(timer.getStatement()));
}

NodePtr NodeGenerator::visitDebugInfo(const ram::DebugInfo& dbg) {
    return mk<DebugInfo>(I_DebugInfo, &dbg, visit(dbg.getStatement()));
}

NodePtr NodeGenerator::visitClear(const ram::Clear& clear) {
    size_t relId = encodeRelation(clear.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType("Clear", lookup(clear.getRelation()));
    return mk<Clear>(type, &clear, rel);
}

NodePtr NodeGenerator::visitLogSize(const ram::LogSize& size) {
    size_t relId = encodeRelation(size.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<LogSize>(I_LogSize, &size, rel);
}

NodePtr NodeGenerator::visitIO(const ram::IO& io) {
    size_t relId = encodeRelation(io.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<IO>(I_IO, &io, rel);
}

NodePtr NodeGenerator::visitQuery(const ram::Query& query) {
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

NodePtr NodeGenerator::visitExtend(const ram::Extend& extend) {
    size_t src = encodeRelation(extend.getFirstRelation());
    size_t target = encodeRelation(extend.getSecondRelation());
    return mk<Extend>(I_Extend, &extend, src, target);
}

NodePtr NodeGenerator::visitSwap(const ram::Swap& swap) {
    size_t src = encodeRelation(swap.getFirstRelation());
    size_t target = encodeRelation(swap.getSecondRelation());
    return mk<Swap>(I_Swap, &swap, src, target);
}

NodePtr NodeGenerator::visitUndefValue(const ram::UndefValue&) {
    return nullptr;
}

NodePtr NodeGenerator::visitNode(const ram::Node& node) {
    fatal("unsupported node type: %s", typeid(node).name());
}

void NodeGenerator::newQueryBlock() {
    viewTable.clear();
    viewId = 0;
}

size_t NodeGenerator::getNewRelId() {
    return relId++;
}

size_t NodeGenerator::getNextViewId() {
    return viewId++;
}

template <class RamNode>
size_t NodeGenerator::encodeIndexPos(RamNode& node) {
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

size_t NodeGenerator::encodeView(const ram::Node* node) {
    auto pos = viewTable.find(node);
    if (pos != viewTable.end()) {
        return pos->second;
    }
    size_t id = getNextViewId();
    viewTable[node] = id;
    return id;
}

const ram::Relation& NodeGenerator::lookup(const std::string& relName) {
    auto it = relationMap.find(relName);
    assert(it != relationMap.end() && "relation not found");
    return *it->second;
}

size_t NodeGenerator::getArity(const std::string& relName) {
    auto rel = lookup(relName);
    return rel.getArity();
}

size_t NodeGenerator::encodeRelation(const std::string& relName) {
    auto pos = relTable.find(relName);
    if (pos != relTable.end()) {
        return pos->second;
    }
    size_t id = getNewRelId();
    relTable[relName] = id;
    engine.createRelation(lookup(relName), id);
    return id;
}

RelationHandle* NodeGenerator::getRelationHandle(const size_t idx) {
    return engine.relations[idx].get();
}

bool NodeGenerator::requireView(const ram::Node* node) {
    if (isA<ram::AbstractExistenceCheck>(node)) {
        return true;
    } else if (isA<ram::IndexOperation>(node)) {
        return true;
    }
    return false;
}

const std::string& NodeGenerator::getViewRelation(const ram::Node* node) {
    if (const auto* exist = dynamic_cast<const ram::AbstractExistenceCheck*>(node)) {
        return exist->getRelation();
    } else if (const auto* index = dynamic_cast<const ram::IndexOperation*>(node)) {
        return index->getRelation();
    }

    fatal("The ram::Node does not require a view.");
}

SuperInstruction NodeGenerator::getIndexSuperInstInfo(const ram::IndexOperation& ramIndex) {
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

SuperInstruction NodeGenerator::getExistenceSuperInstInfo(const ram::AbstractExistenceCheck& abstractExist) {
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

SuperInstruction NodeGenerator::getProjectSuperInstInfo(const ram::Project& exist) {
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

// -- Definition of OrderingContext --

NodeGenerator::OrderingContext::OrderingContext(NodeGenerator& generator) : generator(generator) {}

void NodeGenerator::OrderingContext::addNewTuple(size_t tupleId, size_t arity) {
    std::vector<uint32_t> order;
    for (size_t i = 0; i < arity; ++i) {
        order.push_back((uint32_t)i);
    }
    insertOrder(tupleId, std::move(order));
}

template <class RamNode>
void NodeGenerator::OrderingContext::addTupleWithDefaultOrder(size_t tupleId, const RamNode& node) {
    auto interpreterRel = generator.encodeRelation(node.getRelation());
    insertOrder(tupleId, (*generator.getRelationHandle(interpreterRel))->getIndexOrder(0));
}

template <class RamNode>
void NodeGenerator::OrderingContext::addTupleWithIndexOrder(size_t tupleId, const RamNode& node) {
    auto interpreterRel = generator.encodeRelation(node.getRelation());
    auto indexId = generator.encodeIndexPos(node);
    auto order = (*generator.getRelationHandle(interpreterRel))->getIndexOrder(indexId);
    insertOrder(tupleId, order);
}

size_t NodeGenerator::OrderingContext::mapOrder(size_t tupleId, size_t elementId) const {
    return tupleOrders[tupleId][elementId];
}

void NodeGenerator::OrderingContext::insertOrder(size_t tupleId, const Order& order) {
    if (tupleId >= tupleOrders.size()) {
        tupleOrders.resize(tupleId + 1);
    }

    std::vector<uint32_t> decodeOrder(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        decodeOrder[order[i]] = i;
    }

    tupleOrders[tupleId] = std::move(decodeOrder);
}
};  // namespace souffle::interpreter
