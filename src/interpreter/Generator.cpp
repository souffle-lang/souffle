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
#include "ram/UserDefinedAggregator.h"

namespace souffle::interpreter {

using NodePtr = Own<Node>;
using NodePtrVec = std::vector<NodePtr>;
using RelationHandle = Own<RelationWrapper>;

NodeGenerator::NodeGenerator(Engine& engine) : engine(engine), global(engine.getGlobal()) {
    visit(engine.tUnit.getProgram(), [&](const ram::Relation& relation) {
        assert(relationMap.find(relation.getName()) == relationMap.end() && "double-naming of relations");
        relationMap[relation.getName()] = &relation;
    });
}

NodePtr NodeGenerator::generateTree(const ram::Node& root) {
    // Encode all relation, indexPos and viewId.
    visit(root, [&](const ram::Node& node) {
        if (isA<ram::Query>(&node)) {
            newQueryBlock();
        }
        if (const auto* estimateJoinSize = as<ram::EstimateJoinSize>(node)) {
            encodeIndexPos(*estimateJoinSize);
            encodeView(estimateJoinSize);
        } else if (const auto* indexSearch = as<ram::IndexOperation>(node)) {
            encodeIndexPos(*indexSearch);
            encodeView(indexSearch);
        } else if (const auto* exists = as<ram::ExistenceCheck>(node)) {
            encodeIndexPos(*exists);
            encodeView(exists);
        } else if (const auto* provExists = as<ram::ProvenanceExistenceCheck>(node)) {
            encodeIndexPos(*provExists);
            encodeView(provExists);
        }
    });
    // Parse program
    return dispatch(root);
}

NodePtr NodeGenerator::visit_(type_identity<ram::StringConstant>, const ram::StringConstant& sc) {
    std::size_t num = engine.getSymbolTable().encode(sc.getConstant());
    return mk<StringConstant>(I_StringConstant, &sc, num);
}

NodePtr NodeGenerator::visit_(type_identity<ram::NumericConstant>, const ram::NumericConstant& num) {
    return mk<NumericConstant>(I_NumericConstant, &num);
}

NodePtr NodeGenerator::visit_(type_identity<ram::Variable>, const ram::Variable& var) {
    return mk<Variable>(I_Variable, &var);
}

NodePtr NodeGenerator::visit_(type_identity<ram::TupleElement>, const ram::TupleElement& access) {
    auto tupleId = access.getTupleId();
    auto elementId = access.getElement();
    auto newElementId = orderingContext.mapOrder(tupleId, elementId);
    return mk<TupleElement>(I_TupleElement, &access, tupleId, newElementId);
}

NodePtr NodeGenerator::visit_(type_identity<ram::AutoIncrement>, const ram::AutoIncrement& inc) {
    return mk<AutoIncrement>(I_AutoIncrement, &inc);
}

NodePtr NodeGenerator::visit_(type_identity<ram::IntrinsicOperator>, const ram::IntrinsicOperator& op) {
    NodePtrVec children;
    for (const auto& arg : op.getArguments()) {
        children.push_back(dispatch(*arg));
    }
    return mk<IntrinsicOperator>(I_IntrinsicOperator, &op, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::UserDefinedOperator>, const ram::UserDefinedOperator& op) {
    NodePtrVec children;
    for (const auto& arg : op.getArguments()) {
        children.push_back(dispatch(*arg));
    }

    /* Resolve functor to actual function pointer now */
    void* functionPointer = engine.getMethodHandle(op.getName());

    auto node = mk<UserDefinedOperator>(I_UserDefinedOperator, &op, std::move(children), functionPointer);

#ifdef USE_LIBFFI
    /* Prepare FFI dynamic call structure */

#if RAM_DOMAIN_SIZE == 64
#define FFI_RamSigned ffi_type_sint64
#define FFI_RamUnsigned ffi_type_uint64
#define FFI_RamFloat ffi_type_double
#define FFI_Symbol ffi_type_pointer
#else
#define FFI_RamSigned ffi_type_sint32
#define FFI_RamUnsigned ffi_type_uint32
#define FFI_RamFloat ffi_type_float
#define FFI_Symbol ffi_type_pointer
#endif

    const std::size_t arity = op.getArguments().size();
    const std::size_t nbArgs = arity + (op.isStateful() ? 2 : 0);

    Own<ffi_cif> cif = mk<ffi_cif>();
    Own<ffi_type*[]> args = mk<ffi_type*[]>(nbArgs);
    ffi_type* codomain;

    if (op.isStateful()) {
        args[0] = args[1] = &ffi_type_pointer;
        for (std::size_t i = 0; i < arity; i++) {
            args[i + 2] = &FFI_RamSigned;
        }

        codomain = &FFI_RamSigned;

    } else {
        const std::vector<TypeAttribute>& types = op.getArgsTypes();
        const auto returnType = op.getReturnType();

        for (std::size_t i = 0; i < arity; i++) {
            switch (types[i]) {
                case TypeAttribute::Symbol: args[i] = &FFI_Symbol; break;
                case TypeAttribute::Signed: args[i] = &FFI_RamSigned; break;
                case TypeAttribute::Unsigned: args[i] = &FFI_RamUnsigned; break;
                case TypeAttribute::Float: args[i] = &FFI_RamFloat; break;
                case TypeAttribute::ADT: fatal("ADT support is not implemented");
                case TypeAttribute::Record: fatal("Record support is not implemented");
                default: fatal("Not implemented");
            }
        }

        switch (returnType) {
            case TypeAttribute::Symbol: codomain = &FFI_Symbol; break;
            case TypeAttribute::Signed: codomain = &FFI_RamSigned; break;
            case TypeAttribute::Unsigned: codomain = &FFI_RamUnsigned; break;
            case TypeAttribute::Float: codomain = &FFI_RamFloat; break;
            case TypeAttribute::ADT: fatal("ADT support is not implemented");
            case TypeAttribute::Record: fatal("Record support is not implemented");
            default: fatal("Not implemented");
        }
    }

    const auto prepStatus =
            ffi_prep_cif(cif.get(), FFI_DEFAULT_ABI, static_cast<unsigned int>(nbArgs), codomain, args.get());
    if (prepStatus != FFI_OK) {
        fatal("Failed to prepare CIF for user-defined operator `%s`; error code = %d", op.getName(),
                prepStatus);
    }
    node->setFFI(std::move(cif), std::move(args));
#endif

    return node;
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::NestedIntrinsicOperator>, const ram::NestedIntrinsicOperator& op) {
    auto arity = op.getArguments().size();
    orderingContext.addNewTuple(op.getTupleId(), arity);
    NodePtrVec children;
    for (auto&& arg : op.getArguments()) {
        children.push_back(dispatch(*arg));
    }
    children.push_back(visit_(type_identity<ram::TupleOperation>(), op));
    return mk<NestedIntrinsicOperator>(I_NestedIntrinsicOperator, &op, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::PackRecord>, const ram::PackRecord& pr) {
    NodePtrVec children;
    for (const auto& arg : pr.getArguments()) {
        children.push_back(dispatch(*arg));
    }
    return mk<PackRecord>(I_PackRecord, &pr, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::SubroutineArgument>, const ram::SubroutineArgument& arg) {
    return mk<SubroutineArgument>(I_SubroutineArgument, &arg);
}

// -- connectors operators --
NodePtr NodeGenerator::visit_(type_identity<ram::True>, const ram::True& ltrue) {
    return mk<True>(I_True, &ltrue);
}

NodePtr NodeGenerator::visit_(type_identity<ram::False>, const ram::False& lfalse) {
    return mk<False>(I_False, &lfalse);
}

NodePtr NodeGenerator::visit_(type_identity<ram::Conjunction>, const ram::Conjunction& conj) {
    NodePtrVec children;
    std::stack<const ram::Node*> dfs;
    dfs.push(&conj.getRHS());
    dfs.push(&conj.getLHS());
    while (!dfs.empty()) {
        const ram::Node* term = dfs.top();
        dfs.pop();
        if (const ram::Conjunction* subconj = as<ram::Conjunction>(term)) {
            dfs.push(&subconj->getRHS());
            dfs.push(&subconj->getLHS());
        } else {
            children.emplace_back(dispatch(*term));
        }
    }
    return mk<Conjunction>(I_Conjunction, &conj, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Negation>, const ram::Negation& neg) {
    return mk<Negation>(I_Negation, &neg, dispatch(neg.getOperand()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::EmptinessCheck>, const ram::EmptinessCheck& emptiness) {
    std::size_t relId = encodeRelation(emptiness.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "EmptinessCheck", lookup(emptiness.getRelation()));
    return mk<EmptinessCheck>(type, &emptiness, rel);
}

NodePtr NodeGenerator::visit_(type_identity<ram::RelationSize>, const ram::RelationSize& size) {
    std::size_t relId = encodeRelation(size.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "RelationSize", lookup(size.getRelation()));
    return mk<RelationSize>(type, &size, rel);
}

NodePtr NodeGenerator::visit_(type_identity<ram::ExistenceCheck>, const ram::ExistenceCheck& exists) {
    SuperInstruction superOp = getExistenceSuperInstInfo(exists);
    // Check if the search signature is a total signature
    bool isTotal = true;
    for (const auto& cur : exists.getValues()) {
        if (isUndefValue(cur)) {
            isTotal = false;
        }
    }
    const auto& ramRelation = lookup(exists.getRelation());
    NodeType type = constructNodeType(global, "ExistenceCheck", ramRelation);
    return mk<ExistenceCheck>(type, &exists, isTotal, encodeView(&exists), std::move(superOp),
            ramRelation.isTemp(), ramRelation.getName());
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::ProvenanceExistenceCheck>, const ram::ProvenanceExistenceCheck& provExists) {
    SuperInstruction superOp = getExistenceSuperInstInfo(provExists);
    NodeType type = constructNodeType(global, "ProvenanceExistenceCheck", lookup(provExists.getRelation()));
    return mk<ProvenanceExistenceCheck>(type, &provExists, dispatch(*(--provExists.getChildNodes().end())),
            encodeView(&provExists), std::move(superOp));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Constraint>, const ram::Constraint& relOp) {
    auto left = dispatch(relOp.getLHS());
    auto right = dispatch(relOp.getRHS());
    switch (relOp.getOperator()) {
        case BinaryConstraintOp::MATCH:
        case BinaryConstraintOp::NOT_MATCH:
            if (const StringConstant* str = dynamic_cast<const StringConstant*>(left.get()); str) {
                const std::string& pattern = engine.getSymbolTable().unsafeDecode(str->getConstant());
                try {
                    std::regex regex(pattern);
                    // treat the string constant as a regex
                    left = mk<RegexConstant>(*str, std::move(regex));
                } catch (const std::exception&) {
                    std::cerr << "warning: wrong pattern provided \"" << pattern << "\"\n";

                    // we could not compile the pattern
                    left = mk<RegexConstant>(*str, std::nullopt);
                }
            }
            break;
        default: break;
    }

    return mk<Constraint>(I_Constraint, &relOp, std::move(left), std::move(right));
}

NodePtr NodeGenerator::visit_(type_identity<ram::NestedOperation>, const ram::NestedOperation& nested) {
    return dispatch(nested.getOperation());
}

NodePtr NodeGenerator::visit_(type_identity<ram::TupleOperation>, const ram::TupleOperation& search) {
    if (engine.profileEnabled && engine.frequencyCounterEnabled && !search.getProfileText().empty()) {
        return mk<TupleOperation>(I_TupleOperation, &search, dispatch(search.getOperation()));
    }
    return dispatch(search.getOperation());
}

NodePtr NodeGenerator::visit_(type_identity<ram::Scan>, const ram::Scan& scan) {
    orderingContext.addTupleWithDefaultOrder(scan.getTupleId(), scan);
    std::size_t relId = encodeRelation(scan.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "Scan", lookup(scan.getRelation()));
    return mk<Scan>(type, &scan, rel, visit_(type_identity<ram::TupleOperation>(), scan));
}

NodePtr NodeGenerator::visit_(type_identity<ram::ParallelScan>, const ram::ParallelScan& pScan) {
    orderingContext.addTupleWithDefaultOrder(pScan.getTupleId(), pScan);
    std::size_t relId = encodeRelation(pScan.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "ParallelScan", lookup(pScan.getRelation()));
    auto res = mk<ParallelScan>(type, &pScan, rel, visit_(type_identity<ram::TupleOperation>(), pScan));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visit_(type_identity<ram::IndexScan>, const ram::IndexScan& iScan) {
    orderingContext.addTupleWithIndexOrder(iScan.getTupleId(), iScan);
    SuperInstruction indexOperation = getIndexSuperInstInfo(iScan);
    NodeType type = constructNodeType(global, "IndexScan", lookup(iScan.getRelation()));
    return mk<IndexScan>(type, &iScan, nullptr, visit_(type_identity<ram::TupleOperation>(), iScan),
            encodeView(&iScan), std::move(indexOperation));
}

NodePtr NodeGenerator::visit_(type_identity<ram::ParallelIndexScan>, const ram::ParallelIndexScan& piscan) {
    orderingContext.addTupleWithIndexOrder(piscan.getTupleId(), piscan);
    SuperInstruction indexOperation = getIndexSuperInstInfo(piscan);
    std::size_t relId = encodeRelation(piscan.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "ParallelIndexScan", lookup(piscan.getRelation()));
    auto res = mk<ParallelIndexScan>(type, &piscan, rel, visit_(type_identity<ram::TupleOperation>(), piscan),
            encodeIndexPos(piscan), std::move(indexOperation));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visit_(type_identity<ram::IfExists>, const ram::IfExists& ifexists) {
    orderingContext.addTupleWithDefaultOrder(ifexists.getTupleId(), ifexists);
    std::size_t relId = encodeRelation(ifexists.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "IfExists", lookup(ifexists.getRelation()));
    return mk<IfExists>(type, &ifexists, rel, dispatch(ifexists.getCondition()),
            visit_(type_identity<ram::TupleOperation>(), ifexists));
}

NodePtr NodeGenerator::visit_(type_identity<ram::ParallelIfExists>, const ram::ParallelIfExists& pIfExists) {
    orderingContext.addTupleWithDefaultOrder(pIfExists.getTupleId(), pIfExists);
    std::size_t relId = encodeRelation(pIfExists.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "ParallelIfExists", lookup(pIfExists.getRelation()));
    auto res = mk<ParallelIfExists>(type, &pIfExists, rel, dispatch(pIfExists.getCondition()),
            visit_(type_identity<ram::TupleOperation>(), pIfExists));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visit_(type_identity<ram::IndexIfExists>, const ram::IndexIfExists& iIfExists) {
    orderingContext.addTupleWithIndexOrder(iIfExists.getTupleId(), iIfExists);
    SuperInstruction indexOperation = getIndexSuperInstInfo(iIfExists);
    NodeType type = constructNodeType(global, "IndexIfExists", lookup(iIfExists.getRelation()));
    return mk<IndexIfExists>(type, &iIfExists, nullptr, dispatch(iIfExists.getCondition()),
            visit_(type_identity<ram::TupleOperation>(), iIfExists), encodeView(&iIfExists),
            std::move(indexOperation));
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::ParallelIndexIfExists>, const ram::ParallelIndexIfExists& piIfExists) {
    orderingContext.addTupleWithIndexOrder(piIfExists.getTupleId(), piIfExists);
    SuperInstruction indexOperation = getIndexSuperInstInfo(piIfExists);
    std::size_t relId = encodeRelation(piIfExists.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "ParallelIndexIfExists", lookup(piIfExists.getRelation()));
    auto res = mk<ParallelIndexIfExists>(type, &piIfExists, rel, dispatch(piIfExists.getCondition()),
            dispatch(piIfExists.getOperation()), encodeIndexPos(piIfExists), std::move(indexOperation));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::UnpackRecord>, const ram::UnpackRecord& unpack) {  // get reference
    orderingContext.addNewTuple(unpack.getTupleId(), unpack.getArity());
    return mk<UnpackRecord>(I_UnpackRecord, &unpack, dispatch(unpack.getExpression()),
            visit_(type_identity<ram::TupleOperation>(), unpack));
}

NodePtr NodeGenerator::mkInit(const ram::AbstractAggregate& aggregate) {
    const ram::Aggregator& aggregator = aggregate.getAggregator();
    if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
        return dispatch(*uda->getInitValue());
    } else {
        return nullptr;
    }
};

void* NodeGenerator::resolveFunctionPointers(const ram::AbstractAggregate& aggregate) {
    const ram::Aggregator& aggregator = aggregate.getAggregator();
    if (const auto* uda = as<ram::UserDefinedAggregator>(aggregator)) {
        void* functionPtr = engine.getMethodHandle(uda->getName());
        if (functionPtr == nullptr) {
            fatal("cannot find user-defined operator `%s`", uda->getName());
        }
        return functionPtr;
    } else {
        // Intrinsic aggregates do not need function pointers.
        return nullptr;
    }
}

NodePtr NodeGenerator::visit_(type_identity<ram::Aggregate>, const ram::Aggregate& aggregate) {
    // Notice: Aggregate is sensitive to the visiting order of the subexprs in order to make
    // orderCtxt consistent. The order of visiting should be the same as the order of execution during
    // runtime.
    orderingContext.addTupleWithDefaultOrder(aggregate.getTupleId(), aggregate);
    NodePtr init = mkInit(aggregate);
    NodePtr expr = dispatch(aggregate.getExpression());
    NodePtr cond = dispatch(aggregate.getCondition());
    orderingContext.addNewTuple(aggregate.getTupleId(), 1);
    NodePtr nested = visit_(type_identity<ram::TupleOperation>(), aggregate);
    std::size_t relId = encodeRelation(aggregate.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "Aggregate", lookup(aggregate.getRelation()));

    /* Resolve functor to actual function pointer now */
    void* functionPtr = resolveFunctionPointers(aggregate);

    return mk<Aggregate>(type, &aggregate, rel, std::move(expr), std::move(cond), std::move(nested),
            std::move(init), functionPtr);
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::ParallelAggregate>, const ram::ParallelAggregate& pAggregate) {
    orderingContext.addTupleWithDefaultOrder(pAggregate.getTupleId(), pAggregate);
    NodePtr init = mkInit(pAggregate);
    NodePtr expr = dispatch(pAggregate.getExpression());
    NodePtr cond = dispatch(pAggregate.getCondition());
    orderingContext.addNewTuple(pAggregate.getTupleId(), 1);
    NodePtr nested = visit_(type_identity<ram::TupleOperation>(), pAggregate);
    std::size_t relId = encodeRelation(pAggregate.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "ParallelAggregate", lookup(pAggregate.getRelation()));
    /* Resolve functor to actual function pointer now */
    void* functionPtr = resolveFunctionPointers(pAggregate);
    auto res = mk<ParallelAggregate>(type, &pAggregate, rel, std::move(expr), std::move(cond),
            std::move(nested), std::move(init), functionPtr);
    res->setViewContext(parentQueryViewContext);

    return res;
}

NodePtr NodeGenerator::visit_(type_identity<ram::IndexAggregate>, const ram::IndexAggregate& iAggregate) {
    orderingContext.addTupleWithIndexOrder(iAggregate.getTupleId(), iAggregate);
    SuperInstruction indexOperation = getIndexSuperInstInfo(iAggregate);
    NodePtr init = mkInit(iAggregate);
    NodePtr expr = dispatch(iAggregate.getExpression());
    NodePtr cond = dispatch(iAggregate.getCondition());
    orderingContext.addNewTuple(iAggregate.getTupleId(), 1);
    NodePtr nested = visit_(type_identity<ram::TupleOperation>(), iAggregate);
    std::size_t relId = encodeRelation(iAggregate.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "IndexAggregate", lookup(iAggregate.getRelation()));
    /* Resolve functor to actual function pointer now */
    void* functionPtr = resolveFunctionPointers(iAggregate);
    return mk<IndexAggregate>(type, &iAggregate, rel, std::move(expr), std::move(cond), std::move(nested),
            std::move(init), functionPtr, encodeView(&iAggregate), std::move(indexOperation));
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::ParallelIndexAggregate>, const ram::ParallelIndexAggregate& piAggregate) {
    orderingContext.addTupleWithIndexOrder(piAggregate.getTupleId(), piAggregate);
    SuperInstruction indexOperation = getIndexSuperInstInfo(piAggregate);
    NodePtr init = mkInit(piAggregate);
    NodePtr expr = dispatch(piAggregate.getExpression());
    NodePtr cond = dispatch(piAggregate.getCondition());
    orderingContext.addNewTuple(piAggregate.getTupleId(), 1);
    NodePtr nested = visit_(type_identity<ram::TupleOperation>(), piAggregate);
    std::size_t relId = encodeRelation(piAggregate.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "ParallelIndexAggregate", lookup(piAggregate.getRelation()));
    /* Resolve functor to actual function pointer now */
    void* functionPtr = resolveFunctionPointers(piAggregate);
    auto res = mk<ParallelIndexAggregate>(type, &piAggregate, rel, std::move(expr), std::move(cond),
            std::move(nested), std::move(init), functionPtr, encodeView(&piAggregate),
            std::move(indexOperation));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visit_(type_identity<ram::Break>, const ram::Break& breakOp) {
    return mk<Break>(I_Break, &breakOp, dispatch(breakOp.getCondition()), dispatch(breakOp.getOperation()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Filter>, const ram::Filter& filter) {
    return mk<Filter>(I_Filter, &filter, dispatch(filter.getCondition()), dispatch(filter.getOperation()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::GuardedInsert>, const ram::GuardedInsert& guardedInsert) {
    SuperInstruction superOp = getInsertSuperInstInfo(guardedInsert);
    std::size_t relId = encodeRelation(guardedInsert.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "GuardedInsert", lookup(guardedInsert.getRelation()));
    auto condition = guardedInsert.getCondition();
    return mk<GuardedInsert>(type, &guardedInsert, rel, std::move(superOp), dispatch(*condition));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Insert>, const ram::Insert& insert) {
    SuperInstruction superOp = getInsertSuperInstInfo(insert);
    std::size_t relId = encodeRelation(insert.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "Insert", lookup(insert.getRelation()));
    return mk<Insert>(type, &insert, rel, std::move(superOp));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Erase>, const ram::Erase& erase) {
    SuperInstruction superOp = getEraseSuperInstInfo(erase);
    std::size_t relId = encodeRelation(erase.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "Erase", lookup(erase.getRelation()));
    return mk<Erase>(type, &erase, rel, std::move(superOp));
}

NodePtr NodeGenerator::visit_(type_identity<ram::SubroutineReturn>, const ram::SubroutineReturn& ret) {
    NodePtrVec children;
    for (const auto& value : ret.getValues()) {
        children.push_back(dispatch(*value));
    }
    return mk<SubroutineReturn>(I_SubroutineReturn, &ret, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Sequence>, const ram::Sequence& seq) {
    NodePtrVec children;
    for (const auto& value : seq.getStatements()) {
        children.push_back(dispatch(*value));
    }
    return mk<Sequence>(I_Sequence, &seq, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Parallel>, const ram::Parallel& parallel) {
    // Parallel statements are executed in sequence for now.
    NodePtrVec children;
    for (const auto& value : parallel.getStatements()) {
        children.push_back(dispatch(*value));
    }
    return mk<Parallel>(I_Parallel, &parallel, std::move(children));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Loop>, const ram::Loop& loop) {
    return mk<Loop>(I_Loop, &loop, dispatch(loop.getBody()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Exit>, const ram::Exit& exit) {
    return mk<Exit>(I_Exit, &exit, dispatch(exit.getCondition()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Call>, const ram::Call& call) {
    // translate a subroutine name to an index
    // the index is used to identify the subroutine
    // in the interpreter. The index is stored in the
    // data array of the Node as the first
    // entry.
    return mk<Call>(I_Call, &call, call.getName());
}

NodePtr NodeGenerator::visit_(type_identity<ram::LogRelationTimer>, const ram::LogRelationTimer& timer) {
    std::size_t relId = encodeRelation(timer.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<LogRelationTimer>(I_LogRelationTimer, &timer, dispatch(timer.getStatement()), rel);
}

NodePtr NodeGenerator::visit_(type_identity<ram::LogTimer>, const ram::LogTimer& timer) {
    return mk<LogTimer>(I_LogTimer, &timer, dispatch(timer.getStatement()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::DebugInfo>, const ram::DebugInfo& dbg) {
    return mk<DebugInfo>(I_DebugInfo, &dbg, dispatch(dbg.getStatement()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::Clear>, const ram::Clear& clear) {
    std::size_t relId = encodeRelation(clear.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<Clear>(I_Clear, &clear, rel);
}

NodePtr NodeGenerator::visit_(
        type_identity<ram::EstimateJoinSize>, const ram::EstimateJoinSize& estimateJoinSize) {
    std::size_t relId = encodeRelation(estimateJoinSize.getRelation());
    auto rel = getRelationHandle(relId);
    NodeType type = constructNodeType(global, "EstimateJoinSize", lookup(estimateJoinSize.getRelation()));
    return mk<EstimateJoinSize>(type, &estimateJoinSize, rel, encodeIndexPos(estimateJoinSize));
}

NodePtr NodeGenerator::visit_(type_identity<ram::LogSize>, const ram::LogSize& size) {
    std::size_t relId = encodeRelation(size.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<LogSize>(I_LogSize, &size, rel);
}

NodePtr NodeGenerator::visit_(type_identity<ram::IO>, const ram::IO& io) {
    std::size_t relId = encodeRelation(io.getRelation());
    auto rel = getRelationHandle(relId);
    return mk<IO>(I_IO, &io, rel);
}

NodePtr NodeGenerator::visit_(type_identity<ram::Query>, const ram::Query& query) {
    std::shared_ptr<ViewContext> viewContext = std::make_shared<ViewContext>();
    parentQueryViewContext = viewContext;
    // split terms of conditions of outer-most filter operation
    // into terms that require a context and terms that
    // do not require a view
    const ram::Operation* next = &query.getOperation();
    std::vector<const ram::Condition*> freeOfView;
    if (const auto* filter = as<ram::Filter>(query.getOperation())) {
        next = &filter->getOperation();
        // Check terms of outer filter operation whether they can be pushed before
        // the view-generation for speed improvements
        auto conditions = findConjunctiveTerms(&filter->getCondition());
        for (auto const& cur : conditions) {
            bool needView = false;
            visit(*cur, [&](const ram::Node& node) {
                if (requireView(&node)) {
                    needView = true;
                    const auto& rel = getViewRelation(&node);
                    viewContext->addViewInfoForFilter(
                            encodeRelation(rel), indexTable[&node], encodeView(&node));
                }
            });

            if (needView) {
                viewContext->addViewOperationForFilter(dispatch(*cur));
            } else {
                viewContext->addViewFreeOperationForFilter(dispatch(*cur));
            }
        }
    }

    visit(*next, [&](const ram::Node& node) {
        if (requireView(&node)) {
            const auto& rel = getViewRelation(&node);
            viewContext->addViewInfoForNested(encodeRelation(rel), indexTable[&node], encodeView(&node));
        };
    });

    viewContext->isParallel =
            visitExists(*next, [&](const Node& n) { return as<ram::AbstractParallel, AllowCrossCast>(n); });

    auto res = mk<Query>(I_Query, &query, dispatch(*next));
    res->setViewContext(parentQueryViewContext);
    return res;
}

NodePtr NodeGenerator::visit_(type_identity<ram::MergeExtend>, const ram::MergeExtend& extend) {
    std::size_t src = encodeRelation(extend.getFirstRelation());
    std::size_t target = encodeRelation(extend.getSecondRelation());
    return mk<MergeExtend>(I_MergeExtend, &extend, src, target);
}

NodePtr NodeGenerator::visit_(type_identity<ram::Swap>, const ram::Swap& swap) {
    std::size_t src = encodeRelation(swap.getFirstRelation());
    std::size_t target = encodeRelation(swap.getSecondRelation());
    return mk<Swap>(I_Swap, &swap, src, target);
}

NodePtr NodeGenerator::visit_(type_identity<ram::Assign>, const ram::Assign& assign) {
    return mk<Assign>(I_Assign, &assign, dispatch(assign.getVariable()), dispatch(assign.getValue()));
}

NodePtr NodeGenerator::visit_(type_identity<ram::UndefValue>, const ram::UndefValue&) {
    return nullptr;
}

NodePtr NodeGenerator::visit_(type_identity<ram::Node>, const ram::Node& node) {
    fatal("unsupported node type: %s", typeid(node).name());
}

void NodeGenerator::newQueryBlock() {
    viewTable.clear();
    viewId = 0;
}

std::size_t NodeGenerator::getNewRelId() {
    return relId++;
}

std::size_t NodeGenerator::getNextViewId() {
    return viewId++;
}

template <class RamNode>
std::size_t NodeGenerator::encodeIndexPos(RamNode& node) {
    const std::string& name = node.getRelation();
    ram::analysis::SearchSignature signature = engine.isa.getSearchSignature(&node);
    // A zero signature is equivalent as a full order signature.
    if (signature.empty()) {
        signature = ram::analysis::SearchSignature::getFullSearchSignature(signature.arity());
    }
    auto i = engine.isa.getIndexSelection(name).getLexOrderNum(signature);
    indexTable[&node] = i;
    return i;
};

std::size_t NodeGenerator::encodeView(const ram::Node* node) {
    auto pos = viewTable.find(node);
    if (pos != viewTable.end()) {
        return pos->second;
    }
    std::size_t id = getNextViewId();
    viewTable[node] = id;
    return id;
}

const ram::Relation& NodeGenerator::lookup(const std::string& relName) {
    auto it = relationMap.find(relName);
    assert(it != relationMap.end() && "relation not found");
    return *it->second;
}

std::size_t NodeGenerator::getArity(const std::string& relName) {
    const auto& rel = lookup(relName);
    return rel.getArity();
}

std::size_t NodeGenerator::encodeRelation(const std::string& relName) {
    auto pos = relTable.find(relName);
    if (pos != relTable.end()) {
        return pos->second;
    }
    std::size_t id = getNewRelId();
    relTable[relName] = id;
    engine.createRelation(lookup(relName), id);
    return id;
}

RelationHandle* NodeGenerator::getRelationHandle(const std::size_t idx) {
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
    if (const auto* exist = as<ram::AbstractExistenceCheck>(node)) {
        return exist->getRelation();
    } else if (const auto* index = as<ram::IndexOperation>(node)) {
        return index->getRelation();
    }

    fatal("The ram::Node does not require a view.");
}

SuperInstruction NodeGenerator::getIndexSuperInstInfo(const ram::IndexOperation& ramIndex) {
    std::size_t arity = getArity(ramIndex.getRelation());
    auto interpreterRel = encodeRelation(ramIndex.getRelation());
    auto indexId = encodeIndexPos(ramIndex);
    auto order = (*getRelationHandle(interpreterRel))->getIndexOrder(indexId);
    SuperInstruction indexOperation(arity);
    const auto& first = ramIndex.getRangePattern().first;
    for (std::size_t i = 0; i < arity; ++i) {
        // Note: unlike orderingContext::mapOrder, where we try to decode the order,
        // here we have to encode the order.
        auto& low = first[order[i]];

        // Unbounded
        if (isUndefValue(low)) {
            indexOperation.first[i] = MIN_RAM_SIGNED;
            continue;
        }

        // Constant
        if (isA<ram::NumericConstant>(low)) {
            indexOperation.first[i] = as<ram::NumericConstant>(low)->getConstant();
            continue;
        }

        // TupleElement
        if (isA<ram::TupleElement>(low)) {
            auto lowTuple = as<ram::TupleElement>(low);
            std::size_t tupleId = lowTuple->getTupleId();
            std::size_t elementId = lowTuple->getElement();
            std::size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
            indexOperation.tupleFirst.push_back({i, tupleId, newElementId});
            continue;
        }

        // Generic expression
        indexOperation.exprFirst.push_back(std::pair<std::size_t, Own<Node>>(i, dispatch(*low)));
    }
    const auto& second = ramIndex.getRangePattern().second;
    for (std::size_t i = 0; i < arity; ++i) {
        auto& hig = second[order[i]];

        // Unbounded
        if (isUndefValue(hig)) {
            indexOperation.second[i] = MAX_RAM_SIGNED;
            continue;
        }

        // Constant
        if (isA<ram::NumericConstant>(hig)) {
            indexOperation.second[i] = as<ram::NumericConstant>(hig)->getConstant();
            continue;
        }

        // TupleElement
        if (isA<ram::TupleElement>(hig)) {
            auto highTuple = as<ram::TupleElement>(hig);
            std::size_t tupleId = highTuple->getTupleId();
            std::size_t elementId = highTuple->getElement();
            std::size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
            indexOperation.tupleSecond.push_back({i, tupleId, newElementId});
            continue;
        }

        // Generic expression
        indexOperation.exprSecond.push_back(std::pair<std::size_t, Own<Node>>(i, dispatch(*hig)));
    }
    return indexOperation;
}

SuperInstruction NodeGenerator::getExistenceSuperInstInfo(const ram::AbstractExistenceCheck& abstractExist) {
    auto interpreterRel = encodeRelation(abstractExist.getRelation());
    std::size_t indexId = 0;
    if (isA<ram::ExistenceCheck>(&abstractExist)) {
        indexId = encodeIndexPos(*as<ram::ExistenceCheck>(abstractExist));
    } else if (isA<ram::ProvenanceExistenceCheck>(&abstractExist)) {
        indexId = encodeIndexPos(*as<ram::ProvenanceExistenceCheck>(abstractExist));
    } else {
        fatal("Unrecognized ram::AbstractExistenceCheck.");
    }
    auto order = (*getRelationHandle(interpreterRel))->getIndexOrder(indexId);
    std::size_t arity = getArity(abstractExist.getRelation());
    SuperInstruction superOp(arity);
    const auto& children = abstractExist.getValues();
    for (std::size_t i = 0; i < arity; ++i) {
        auto& child = children[order[i]];

        // Unbounded
        if (isUndefValue(child)) {
            superOp.first[i] = MIN_RAM_SIGNED;
            superOp.second[i] = MAX_RAM_SIGNED;
            continue;
        }

        // Constant
        if (isA<ram::NumericConstant>(child)) {
            superOp.first[i] = as<ram::NumericConstant>(child)->getConstant();
            superOp.second[i] = superOp.first[i];
            continue;
        }

        // TupleElement
        if (isA<ram::TupleElement>(child)) {
            auto tuple = as<ram::TupleElement>(child);
            std::size_t tupleId = tuple->getTupleId();
            std::size_t elementId = tuple->getElement();
            std::size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
            superOp.tupleFirst.push_back({i, tupleId, newElementId});
            continue;
        }

        // Generic expression
        superOp.exprFirst.push_back(std::pair<std::size_t, Own<Node>>(i, dispatch(*child)));
    }
    return superOp;
}

SuperInstruction NodeGenerator::getInsertSuperInstInfo(const ram::Insert& exist) {
    std::size_t arity = getArity(exist.getRelation());
    SuperInstruction superOp(arity);
    const auto& children = exist.getValues();
    for (std::size_t i = 0; i < arity; ++i) {
        auto& child = children[i];
        // Constant
        if (isA<ram::NumericConstant>(child)) {
            superOp.first[i] = as<ram::NumericConstant>(child)->getConstant();
            continue;
        }

        // TupleElement
        if (isA<ram::TupleElement>(child)) {
            auto tuple = as<ram::TupleElement>(child);
            std::size_t tupleId = tuple->getTupleId();
            std::size_t elementId = tuple->getElement();
            std::size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
            superOp.tupleFirst.push_back({i, tupleId, newElementId});
            continue;
        }

        // Generic expression
        superOp.exprFirst.push_back(std::pair<std::size_t, Own<Node>>(i, dispatch(*child)));
    }
    return superOp;
}

SuperInstruction NodeGenerator::getEraseSuperInstInfo(const ram::Erase& exist) {
    std::size_t arity = getArity(exist.getRelation());
    SuperInstruction superOp(arity);
    const auto& children = exist.getValues();
    for (std::size_t i = 0; i < arity; ++i) {
        auto& child = children[i];
        // Constant
        if (isA<ram::NumericConstant>(child)) {
            superOp.first[i] = as<ram::NumericConstant>(child)->getConstant();
            continue;
        }

        // TupleElement
        if (isA<ram::TupleElement>(child)) {
            auto tuple = as<ram::TupleElement>(child);
            std::size_t tupleId = tuple->getTupleId();
            std::size_t elementId = tuple->getElement();
            std::size_t newElementId = orderingContext.mapOrder(tupleId, elementId);
            superOp.tupleFirst.push_back({i, tupleId, newElementId});
            continue;
        }

        // Generic expression
        superOp.exprFirst.push_back(std::pair<std::size_t, Own<Node>>(i, dispatch(*child)));
    }
    return superOp;
}

// -- Definition of OrderingContext --

NodeGenerator::OrderingContext::OrderingContext(NodeGenerator& generator) : generator(generator) {}

void NodeGenerator::OrderingContext::addNewTuple(std::size_t tupleId, std::size_t arity) {
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < arity; ++i) {
        order.push_back(i);
    }
    insertOrder(tupleId, order);
}

template <class RamNode>
void NodeGenerator::OrderingContext::addTupleWithDefaultOrder(std::size_t tupleId, const RamNode& node) {
    auto interpreterRel = generator.encodeRelation(node.getRelation());
    insertOrder(tupleId, (*generator.getRelationHandle(interpreterRel))->getIndexOrder(0));
}

template <class RamNode>
void NodeGenerator::OrderingContext::addTupleWithIndexOrder(std::size_t tupleId, const RamNode& node) {
    auto interpreterRel = generator.encodeRelation(node.getRelation());
    auto indexId = generator.encodeIndexPos(node);
    auto order = (*generator.getRelationHandle(interpreterRel))->getIndexOrder(indexId);
    insertOrder(tupleId, order);
}

std::size_t NodeGenerator::OrderingContext::mapOrder(std::size_t tupleId, std::size_t elementId) const {
    return tupleOrders[tupleId][elementId];
}

void NodeGenerator::OrderingContext::insertOrder(std::size_t tupleId, const Order& order) {
    if (tupleId >= tupleOrders.size()) {
        tupleOrders.resize(tupleId + 1);
    }

    std::vector<std::size_t> decodeOrder(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        decodeOrder[order[i]] = i;
    }

    tupleOrders[tupleId] = std::move(decodeOrder);
}
};  // namespace souffle::interpreter
