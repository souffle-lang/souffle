/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Node.h
 *
 * Declares the Interpreter Node class. The interpreter node class
 * is a compact executable representation of RAM nodes for interpretation.
 * There are two main reasons for the class:
 *  - node types are exposed in form of enums so that fast switch-statements
 *    can be employed for interpretation (visitor patterns with their
 *    double-dispatch are too slow).
 *  - nodes are decorated with data so that frequent on-the-fly data-structure
 *    lookups are avoided.
 * Every interpreter node is associated with a unique RAM node.
 ***********************************************************************/

#pragma once

#include "interpreter/Util.h"
#include "ram/Relation.h"
#include "souffle/RamTypes.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"

#ifdef USE_LIBFFI
#include <ffi.h>
#endif

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace souffle {
namespace ram {
class Node;
}

namespace interpreter {
class ViewContext;
struct RelationWrapper;

// clang-format off

/* This macro defines all the interpreterNode token. 
 * For common operation, pass to Forward. 
 * For specialized operation, pass to FOR_EACH(Expand, tok)
 */
#define FOR_EACH_INTERPRETER_TOKEN(Forward, Expand)\
    Forward(NumericConstant)\
    Forward(StringConstant)\
    Forward(TupleElement)\
    Forward(AutoIncrement)\
    Forward(IterationNumber)\
    Forward(IntrinsicOperator)\
    Forward(UserDefinedOperator)\
    Forward(NestedIntrinsicOperator)\
    Forward(PackRecord)\
    Forward(SubroutineArgument)\
    Forward(True)\
    Forward(False)\
    Forward(Conjunction)\
    Forward(Negation)\
    FOR_EACH(Expand, EmptinessCheck)\
    FOR_EACH(Expand, RelationSize)\
    FOR_EACH(Expand, ExistenceCheck)\
    FOR_EACH_PROVENANCE(Expand, ProvenanceExistenceCheck)\
    Forward(Constraint)\
    Forward(TupleOperation)\
    FOR_EACH(Expand, Scan)\
    FOR_EACH(Expand, ParallelScan)\
    FOR_EACH(Expand, IndexScan)\
    FOR_EACH(Expand, ParallelIndexScan)\
    FOR_EACH(Expand, IfExists)\
    FOR_EACH(Expand, ParallelIfExists)\
    FOR_EACH(Expand, IndexIfExists)\
    FOR_EACH(Expand, ParallelIndexIfExists)\
    Forward(UnpackRecord)\
    FOR_EACH(Expand, Aggregate)\
    FOR_EACH(Expand, ParallelAggregate)\
    FOR_EACH(Expand, IndexAggregate)\
    FOR_EACH(Expand, ParallelIndexAggregate)\
    Forward(Break)\
    Forward(Filter)\
    FOR_EACH(Expand, GuardedInsert)\
    FOR_EACH(Expand, Insert)\
    FOR_EACH_BTREE_DELETE(Expand, Erase)\
    Forward(SubroutineReturn)\
    Forward(Sequence)\
    Forward(Parallel)\
    Forward(Loop)\
    Forward(Exit)\
    Forward(LogRelationTimer)\
    Forward(LogTimer)\
    Forward(DebugInfo)\
    FOR_EACH(Expand, Clear)\
    Forward(LogSize)\
    Forward(IO)\
    Forward(Query)\
    Forward(MergeExtend)\
    Forward(Swap)\
    Forward(Call)

#define SINGLE_TOKEN(tok) I_##tok,

#define EXPAND_TOKEN(structure, arity, tok)\
    I_##tok##_##structure##_##arity,

/* 
 * Declares all the tokens.
 * For Forward token OP, creates I_OP
 * For Extended token OP, generate I_OP_Structure_Arity for each data structure and supported arity.
 */
enum NodeType {
    FOR_EACH_INTERPRETER_TOKEN(SINGLE_TOKEN, EXPAND_TOKEN)
};

#undef SINGLE_TOKEN
#undef EXPAND_TOKEN

#define __TO_STRING(a) #a
#define SINGLE_TOKEN_ENTRY(tok) {__TO_STRING(I_##tok), I_##tok},
#define EXPAND_TOKEN_ENTRY(Structure, arity, tok) \
    {__TO_STRING(I_##tok##_##Structure##_##arity), I_##tok##_##Structure##_##arity},

/**
 * Construct interpreterNodeType by looking at the representation and the arity of the given rel.
 *
 * Add reflective from string to NodeType.
 */
inline NodeType constructNodeType(std::string tokBase, const ram::Relation& rel) {
    static bool isProvenance = Global::config().has("provenance");

    static const std::unordered_map<std::string, NodeType> map = {
            FOR_EACH_INTERPRETER_TOKEN(SINGLE_TOKEN_ENTRY, EXPAND_TOKEN_ENTRY)
    };

    std::string arity = std::to_string(rel.getArity());
    if (rel.getRepresentation() == RelationRepresentation::EQREL) {
        return map.at("I_" + tokBase + "_Eqrel_" + arity);
    } else if(rel.getRepresentation() == RelationRepresentation::BTREE_DELETE) { 
        return map.at("I_" + tokBase + "_BtreeDelete_" + arity);
    } else if (isProvenance) {
        return map.at("I_" + tokBase + "_Provenance_" + arity);
    } else  {
        return map.at("I_" + tokBase + "_Btree_" + arity);
    }

    fatal("Unrecognized node type: base:%s arity:%s.", tokBase, arity);
}

#undef __TO_STRING
#undef EXPAND_TOKEN_ENTRY
#undef SINGLE_TOKEN_ENTRY

// clang-format on

/**
 * @class Node
 * @brief This is a shadow node for a ram::Node that is enriched for
 *        with local information so that the interpreter is executing
 *        quickly.
 */

class Node {
public:
    Node(enum NodeType ty, const ram::Node* sdw) : type(ty), shadow(sdw) {}
    virtual ~Node() = default;

    /** @brief get node type */
    inline enum NodeType getType() const {
        return type;
    }

    /** @brief get shadow node, i.e., RAM node */
    inline const ram::Node* getShadow() const {
        return shadow;
    }

protected:
    enum NodeType type;
    const ram::Node* shadow;
};

/**
 * @class CompoundNode
 * @brief Compound node represents interpreter node with a list of children.
 */
class CompoundNode : public Node {
    using NodePtrVec = VecOwn<Node>;

public:
    CompoundNode(enum NodeType ty, const ram::Node* sdw, NodePtrVec children = {})
            : Node(ty, sdw), children(std::move(children)) {}

    /** @brief get children of node */
    inline const Node* getChild(std::size_t i) const {
        return children[i].get();
    }

    /** @brief get list of all children */
    const NodePtrVec& getChildren() const {
        return children;
    }

protected:
    NodePtrVec children;
};

/**
 * @class UnaryNode
 * @brief Unary node represents interpreter node with a single child
 */
class UnaryNode : public Node {
public:
    UnaryNode(enum NodeType ty, const ram::Node* sdw, Own<Node> child)
            : Node(ty, sdw), child(std::move(child)) {}

    inline const Node* getChild() const {
        return child.get();
    }

protected:
    Own<Node> child;
};

/**
 * @class BinaryNode
 * @brief Binary node represents interpreter node with two children.
 */
class BinaryNode : public Node {
public:
    BinaryNode(enum NodeType ty, const ram::Node* sdw, Own<Node> lhs, Own<Node> rhs)
            : Node(ty, sdw), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    /** @brief get left child of node */
    inline const Node* getLhs() const {
        return lhs.get();
    }

    /** @brief get right child of node */
    inline const Node* getRhs() const {
        return rhs.get();
    }

protected:
    Own<Node> lhs;
    Own<Node> rhs;
};

/**
 * @class SuperInstruction
 * @brief This class encodes information for a super-instruction, which is
 *        used to eliminate Number and TupleElement in index/insert/existence operation.
 */
class SuperInstruction {
public:
    SuperInstruction(std::size_t i) {
        first.resize(i);
        second.resize(i);
    }

    SuperInstruction(const SuperInstruction&) = delete;
    SuperInstruction& operator=(const SuperInstruction&) = delete;
    SuperInstruction(SuperInstruction&&) = default;

    /** @brief constant value in the lower bound/pattern */
    std::vector<RamDomain> first;
    /** @brief constant value in the upper bound */
    std::vector<RamDomain> second;
    /** @brief Encoded tupleElement expressions in the lower bound/pattern */
    std::vector<std::array<std::size_t, 3>> tupleFirst;
    /** @brief Encoded tupleElement expressions in the upper bound */
    std::vector<std::array<std::size_t, 3>> tupleSecond;
    /** @brief Generic expressions in the lower bound/pattern */
    std::vector<std::pair<std::size_t, Own<Node>>> exprFirst;
    /** @brief Generic expressions in the upper bound */
    std::vector<std::pair<std::size_t, Own<Node>>> exprSecond;
};

/**
 * @class SuperOperation
 * @brief  node that utilizes the super instruction optimization should
 *        inherit from this class. E.g. ExistenceCheck, Insert
 */
class SuperOperation {
public:
    SuperOperation(SuperInstruction superInst) : superInst(std::move(superInst)) {}

    const SuperInstruction& getSuperInst() const {
        return superInst;
    }

protected:
    const SuperInstruction superInst;
};

/**
 * @class AbstractParallel
 * @brief  node that utilizes parallel execution should inherit from this class.
 *        Enable node with its own view context for parallel execution.
 */
class AbstractParallel {
public:
    /** @brief get view context for operations */
    inline ViewContext* getViewContext() const {
        return viewContext.get();
    }

    /** @brief set view context */
    inline void setViewContext(const std::shared_ptr<ViewContext>& v) {
        viewContext = v;
    }

protected:
    std::shared_ptr<ViewContext> viewContext = nullptr;
};

/**
 * @class ViewOperation
 * @brief  operation that utilizes the index view from underlying relation should inherit from this
 *        class.
 */
class ViewOperation {
public:
    ViewOperation(std::size_t id) : viewId(id) {}

    inline std::size_t getViewId() const {
        return viewId;
    }

protected:
    std::size_t viewId;
};

/**
 * @class BinRelOperation
 * @brief  operation that involves with two relations should inherit from this class.
 *        E.g. Swap, MergeExtend
 */
class BinRelOperation {
public:
    BinRelOperation(std::size_t src, std::size_t target) : src(src), target(target) {}

    inline std::size_t getSourceId() const {
        return src;
    }

    inline std::size_t getTargetId() const {
        return target;
    }

protected:
    const std::size_t src;
    const std::size_t target;
};

/**
 * @class NestedOperation
 * @brief Encode a nested operation for the interpreter node. E.g. Loop, IndexScan
 */
class NestedOperation {
public:
    NestedOperation(Own<Node> nested) : nested(std::move(nested)) {}

    inline const Node* getNestedOperation() const {
        return nested.get();
    };

protected:
    Own<Node> nested;
};

/**
 * @class ConditionalOperation
 * @brief Encode a conditional operation for the interpreter node. E.g. Exit, Filter
 */
class ConditionalOperation {
public:
    ConditionalOperation(Own<Node> cond) : cond(std::move(cond)) {}

    inline const Node* getCondition() const {
        return cond.get();
    };

protected:
    Own<Node> cond;
};

/**
 * @class RelationalOperation
 * @brief Interpreter operation that holds a single relation
 */
class RelationalOperation {
public:
    using RelationHandle = Own<RelationWrapper>;
    RelationalOperation(RelationHandle* relHandle) : relHandle(relHandle) {}

    /** @brief get relation from handle */
    RelationWrapper* getRelation() const {
        assert(relHandle && "No relation cached\n");
        return (*relHandle).get();
    }

private:
    RelationHandle* const relHandle;
};

/**
 * @class NumericConstant
 */
class NumericConstant : public Node {
    using Node::Node;
};

/**
 * @class StringConstant
 */
class StringConstant : public Node {
public:
    StringConstant(NodeType ty, const ram::Node* sdw, std::size_t constant)
            : Node(ty, sdw), constant(constant) {}
    std::size_t getConstant() const {
        return constant;
    }

private:
    std::size_t constant;
};

/**
 * @class TupleElement
 */
class TupleElement : public Node {
public:
    TupleElement(enum NodeType ty, const ram::Node* sdw, std::size_t tupleId, std::size_t elementId)
            : Node(ty, sdw), tupleId(tupleId), element(elementId) {}

    std::size_t getTupleId() const {
        return tupleId;
    }

    std::size_t getElement() const {
        return element;
    }

private:
    std::size_t tupleId;
    std::size_t element;
};

/**
 * @class AutoIncrement
 */
class AutoIncrement : public Node {
    using Node::Node;
};

/**
 * @class IterationNumber
 */
class IterationNumber : public Node {
    using Node::Node;
};

/**
 * @class IntrinsicOperator
 */
class IntrinsicOperator : public CompoundNode {
    using CompoundNode::CompoundNode;
};

/**
 * @class UserDefinedOperator
 */
class UserDefinedOperator : public CompoundNode {
public:
    UserDefinedOperator(NodeType ty, const ram::Node* sdw, VecOwn<Node> children, void* functionPointer)
            : CompoundNode(ty, sdw, std::move(children)), functionPointer(functionPointer) {
#ifdef USE_LIBFFI
        cif = mk<ffi_cif>();
#endif
    }

    void* getFunctionPointer() const {
        return functionPointer;
    }

#ifdef USE_LIBFFI
    ffi_cif* getFFIcif() const {
        return cif.get();
    }

    void setFFI(Own<ffi_cif> c, Own<ffi_type*[]> a) {
        cif = std::move(c);
        args = std::move(a);
    }
#endif

private:
    void* functionPointer;
#ifdef USE_LIBFFI
    Own<ffi_cif> cif;
    Own<ffi_type*[]> args;
#endif
};

/**
 * @class NestedIntrinsicOperator
 */
class NestedIntrinsicOperator : public CompoundNode {
    using CompoundNode::CompoundNode;
};

/**
 * @class PackRecord
 */
class PackRecord : public CompoundNode {
    using CompoundNode::CompoundNode;
};

/**
 * @class SubroutineArgument
 */
class SubroutineArgument : public Node {
    using Node::Node;
};

/**
 * @class True
 */
class True : public Node {
    using Node::Node;
};

/**
 * @class False
 */
class False : public Node {
    using Node::Node;
};

/**
 * @class Conjunction
 */
class Conjunction : public BinaryNode {
    using BinaryNode::BinaryNode;
};

/**
 * @class Negation
 */
class Negation : public UnaryNode {
    using UnaryNode::UnaryNode;
};

/**
 * @class EmptinessCheck
 */
class EmptinessCheck : public Node, public RelationalOperation {
public:
    EmptinessCheck(enum NodeType ty, const ram::Node* sdw, RelationHandle* handle)
            : Node(ty, sdw), RelationalOperation(handle) {}
};

/**
 * @class RelationSize
 */
class RelationSize : public Node, public RelationalOperation {
public:
    RelationSize(enum NodeType ty, const ram::Node* sdw, RelationHandle* handle)
            : Node(ty, sdw), RelationalOperation(handle) {}
};

/**
 * @class ExistenceCheck
 */
class ExistenceCheck : public Node, public SuperOperation, public ViewOperation {
public:
    ExistenceCheck(enum NodeType ty, const ram::Node* sdw, bool totalSearch, std::size_t viewId,
            SuperInstruction superInst, bool tempRelation, std::string relationName)
            : Node(ty, sdw), SuperOperation(std::move(superInst)), ViewOperation(viewId),
              totalSearch(totalSearch), tempRelation(tempRelation), relationName(std::move(relationName)) {}

    bool isTotalSearch() const {
        return totalSearch;
    }

    bool isTemp() const {
        return tempRelation;
    }

    const std::string& getRelationName() const {
        return relationName;
    }

private:
    const bool totalSearch;
    const bool tempRelation;
    const std::string relationName;
};

/**
 * @class ProvenanceExistenceCheck
 */
class ProvenanceExistenceCheck : public UnaryNode, public SuperOperation, public ViewOperation {
public:
    ProvenanceExistenceCheck(enum NodeType ty, const ram::Node* sdw, Own<Node> child, std::size_t viewId,
            SuperInstruction superInst)
            : UnaryNode(ty, sdw, std::move(child)), SuperOperation(std::move(superInst)),
              ViewOperation(viewId) {}
};

/**
 * @class Constraint
 */
class Constraint : public BinaryNode {
    using BinaryNode::BinaryNode;
};

/**
 * @class TupleOperation
 */
class TupleOperation : public UnaryNode {
    using UnaryNode::UnaryNode;
};

/**
 * @class Scan
 */
class Scan : public Node, public NestedOperation, public RelationalOperation {
public:
    Scan(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, Own<Node> nested)
            : Node(ty, sdw), NestedOperation(std::move(nested)), RelationalOperation(relHandle) {}
};

/**
 * @class ParallelScan
 */
class ParallelScan : public Scan, public AbstractParallel {
    using Scan::Scan;
};

/**
 * @class IndexScan
 */
class IndexScan : public Scan, public SuperOperation, public ViewOperation {
public:
    IndexScan(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, Own<Node> nested,
            std::size_t viewId, SuperInstruction superInst)
            : Scan(ty, sdw, relHandle, std::move(nested)), SuperOperation(std::move(superInst)),
              ViewOperation(viewId) {}
};

/**
 * @class ParallelIndexScan
 */
class ParallelIndexScan : public IndexScan, public AbstractParallel {
public:
    using IndexScan::IndexScan;
};

/**
 * @class IfExists
 */
class IfExists : public Node,
                 public ConditionalOperation,
                 public NestedOperation,
                 public RelationalOperation {
public:
    IfExists(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, Own<Node> cond,
            Own<Node> nested)
            : Node(ty, sdw), ConditionalOperation(std::move(cond)), NestedOperation(std::move(nested)),
              RelationalOperation(relHandle) {}
};

/**
 * @class ParallelIfExists
 */
class ParallelIfExists : public IfExists, public AbstractParallel {
    using IfExists::IfExists;
};

/**
 * @class IndexIfExists
 */
class IndexIfExists : public IfExists, public SuperOperation, public ViewOperation {
public:
    IndexIfExists(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, Own<Node> cond,
            Own<Node> nested, std::size_t viewId, SuperInstruction superInst)
            : IfExists(ty, sdw, relHandle, std::move(cond), std::move(nested)),
              SuperOperation(std::move(superInst)), ViewOperation(viewId) {}
};

/**
 * @class ParallelIndexIfExists
 */
class ParallelIndexIfExists : public IndexIfExists, public AbstractParallel {
    using IndexIfExists::IndexIfExists;
};

/**
 * @class UnpackRecord
 */
class UnpackRecord : public Node, public NestedOperation {
public:
    UnpackRecord(enum NodeType ty, const ram::Node* sdw, Own<Node> expr, Own<Node> nested)
            : Node(ty, sdw), NestedOperation(std::move(nested)), expr(std::move(expr)) {}

    inline const Node* getExpr() const {
        return expr.get();
    }

protected:
    Own<Node> expr;
};

/**
 * @class Aggregate
 */
class Aggregate : public Node,
                  public ConditionalOperation,
                  public NestedOperation,
                  public RelationalOperation {
public:
    Aggregate(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, Own<Node> expr,
            Own<Node> filter, Own<Node> nested)
            : Node(ty, sdw), ConditionalOperation(std::move(filter)), NestedOperation(std::move(nested)),
              RelationalOperation(relHandle), expr(std::move(expr)) {}

    inline const Node* getExpr() const {
        return expr.get();
    }

protected:
    Own<Node> expr;
};

/**
 * @class ParallelAggregate
 */
class ParallelAggregate : public Aggregate, public AbstractParallel {
    using Aggregate::Aggregate;
};

/**
 * @class IndexAggregate
 */
class IndexAggregate : public Aggregate, public SuperOperation, public ViewOperation {
public:
    IndexAggregate(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, Own<Node> expr,
            Own<Node> filter, Own<Node> nested, std::size_t viewId, SuperInstruction superInst)
            : Aggregate(ty, sdw, relHandle, std::move(expr), std::move(filter), std::move(nested)),
              SuperOperation(std::move(superInst)), ViewOperation(viewId) {}
};

/**
 * @class ParallelIndexAggregate
 */
class ParallelIndexAggregate : public IndexAggregate, public AbstractParallel {
    using IndexAggregate::IndexAggregate;
};

/**
 * @class Break
 */
class Break : public Node, public ConditionalOperation, public NestedOperation {
public:
    Break(enum NodeType ty, const ram::Node* sdw, Own<Node> cond, Own<Node> nested)
            : Node(ty, sdw), ConditionalOperation(std::move(cond)), NestedOperation(std::move(nested)) {}
};

/**
 * @class Filter
 */
class Filter : public Node, public ConditionalOperation, public NestedOperation {
public:
    Filter(enum NodeType ty, const ram::Node* sdw, Own<Node> cond, Own<Node> nested)
            : Node(ty, sdw), ConditionalOperation(std::move(cond)), NestedOperation(std::move(nested)) {}
};

/**
 * @class Insert
 */
class Insert : public Node, public SuperOperation, public RelationalOperation {
public:
    Insert(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, SuperInstruction superInst)
            : Node(ty, sdw), SuperOperation(std::move(superInst)), RelationalOperation(relHandle) {}
};

/**
 * @class Erase
 */
class Erase : public Node, public SuperOperation, public RelationalOperation {
public:
    Erase(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle, SuperInstruction superInst)
            : Node(ty, sdw), SuperOperation(std::move(superInst)), RelationalOperation(relHandle) {}
};

/**
 * @class GuardedInsert
 */
class GuardedInsert : public Insert, public ConditionalOperation {
public:
    GuardedInsert(enum NodeType ty, const ram::Node* sdw, RelationHandle* relHandle,
            SuperInstruction superInst, Own<Node> condition)
            : Insert(ty, sdw, relHandle, std::move(superInst)), ConditionalOperation(std::move(condition)) {}
};

/**
 * @class SubroutineReturn
 */
class SubroutineReturn : public CompoundNode {
    using CompoundNode::CompoundNode;
};

/**
 * @class Sequence
 */
class Sequence : public CompoundNode {
    using CompoundNode::CompoundNode;
};

/**
 * @class Parallel
 */
class Parallel : public CompoundNode {
    using CompoundNode::CompoundNode;
};

/**
 * @class Loop
 */
class Loop : public UnaryNode {
    using UnaryNode::UnaryNode;
};

/**
 * @class Exit
 */
class Exit : public UnaryNode {
    using UnaryNode::UnaryNode;
};

/**
 * @class LogRelationTimer
 */
class LogRelationTimer : public UnaryNode, public RelationalOperation {
public:
    LogRelationTimer(enum NodeType ty, const ram::Node* sdw, Own<Node> child, RelationHandle* handle)
            : UnaryNode(ty, sdw, std::move(child)), RelationalOperation(handle) {}
};

/**
 * @class LogTimer
 */
class LogTimer : public UnaryNode {
    using UnaryNode::UnaryNode;
};

/**
 * @class DebugInfo
 */
class DebugInfo : public UnaryNode {
    using UnaryNode::UnaryNode;
};

/**
 * @class Clear
 */
class Clear : public Node, public RelationalOperation {
public:
    Clear(enum NodeType ty, const ram::Node* sdw, RelationHandle* handle)
            : Node(ty, sdw), RelationalOperation(handle) {}
};

/**
 * @class Call
 */
class Call : public Node {
public:
    Call(enum NodeType ty, const ram::Node* sdw, std::size_t subroutineId)
            : Node(ty, sdw), subroutineId(subroutineId) {}

    std::size_t getSubroutineId() const {
        return subroutineId;
    }

private:
    const std::size_t subroutineId;
};

/**
 * @class LogSize
 */
class LogSize : public Node, public RelationalOperation {
public:
    LogSize(enum NodeType ty, const ram::Node* sdw, RelationHandle* handle)
            : Node(ty, sdw), RelationalOperation(handle) {}
};

/**
 * @class IO
 */
class IO : public Node, public RelationalOperation {
public:
    IO(enum NodeType ty, const ram::Node* sdw, RelationHandle* handle)
            : Node(ty, sdw), RelationalOperation(handle) {}
};

/**
 * @class Query
 */
class Query : public UnaryNode, public AbstractParallel {
    using UnaryNode::UnaryNode;
};

/**
 * @class MergeExtend
 */
class MergeExtend : public Node, public BinRelOperation {
public:
    MergeExtend(enum NodeType ty, const ram::Node* sdw, std::size_t src, std::size_t target)
            : Node(ty, sdw), BinRelOperation(src, target) {}
};

/**
 * @class Swap
 */
class Swap : public Node, public BinRelOperation {
public:
    Swap(enum NodeType ty, const ram::Node* sdw, std::size_t src, std::size_t target)
            : Node(ty, sdw), BinRelOperation(src, target) {}
};

}  // namespace interpreter
}  // namespace souffle
