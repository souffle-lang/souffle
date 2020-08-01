/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterNode.h
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

#include "RamTypes.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {
class InterpreterPreamble;
class InterpreterRelation;
class RamNode;

enum InterpreterNodeType {
    I_Constant,
    I_TupleElement,
    I_AutoIncrement,
    I_IntrinsicOperator,
    I_UserDefinedOperator,
    I_NestedIntrinsicOperator,
    I_PackRecord,
    I_SubroutineArgument,
    I_True,
    I_False,
    I_Conjunction,
    I_Negation,
    I_EmptinessCheck,
    I_ExistenceCheck,
    I_ProvenanceExistenceCheck,
    I_Constraint,
    I_TupleOperation,
    I_Scan,
    I_ParallelScan,
    I_IndexScan,
    I_ParallelIndexScan,
    I_Choice,
    I_ParallelChoice,
    I_IndexChoice,
    I_ParallelIndexChoice,
    I_UnpackRecord,
    I_Aggregate,
    I_ParallelAggregate,
    I_IndexAggregate,
    I_ParallelIndexAggregate,
    I_Break,
    I_Filter,
    I_Project,
    I_SubroutineReturn,
    I_Sequence,
    I_Parallel,
    I_Loop,
    I_Exit,
    I_LogRelationTimer,
    I_LogTimer,
    I_DebugInfo,
    I_Clear,
    I_LogSize,
    I_IO,
    I_Query,
    I_Extend,
    I_Swap,
    I_Call
};

/**
 * @class InterpreterNode
 * @brief This is a shadow node for a RamNode that is enriched for
 *        with local information so that the interpreter is executing
 *        quickly.
 */

class InterpreterNode {
public:
    using RelationHandle = std::unique_ptr<InterpreterRelation>;

    InterpreterNode(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle = nullptr)
            : type(ty), shadow(sdw), relHandle(relHandle) {}
    virtual ~InterpreterNode() = default;

    /** @brief get node type */
    inline enum InterpreterNodeType getType() const {
        return type;
    }

    /** @brief get shadow node, i.e., RAM node */
    inline const RamNode* getShadow() const {
        return shadow;
    }

    /** @brief get relation from handle */
    InterpreterRelation* getRelation() const {
        assert(relHandle && "No relation cached\n");
        return (*relHandle).get();
    }

protected:
    enum InterpreterNodeType type;
    const RamNode* shadow;
    RelationHandle* const relHandle;
};

/**
 * @class InterpreterCompoundNode
 * @brief Compound node represents interpreter node with a list of children.
 */
class InterpreterCompoundNode : public InterpreterNode {
    using NodePtrVec = std::vector<std::unique_ptr<InterpreterNode>>;

public:
    InterpreterCompoundNode(enum InterpreterNodeType ty, const RamNode* sdw, NodePtrVec children = {},
            RelationHandle* relHandle = nullptr)
            : InterpreterNode(ty, sdw, relHandle), children(std::move(children)) {}

    /** @brief get children of node */
    inline const InterpreterNode* getChild(std::size_t i) const {
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
 * @class InterpreterUnaryNode
 * @brief Unary node represents interpreter node with a single child
 */
class InterpreterUnaryNode : public InterpreterNode {
public:
    InterpreterUnaryNode(enum InterpreterNodeType ty, const RamNode* sdw,
            std::unique_ptr<InterpreterNode> child, RelationHandle* relHandle = nullptr)
            : InterpreterNode(ty, sdw, relHandle), child(std::move(child)) {}

    inline const InterpreterNode* getChild() const {
        return child.get();
    }

protected:
    std::unique_ptr<InterpreterNode> child;
};

/**
 * @class InterpreterBinaryNode
 * @brief Binary node represents interpreter node with two children.
 */
class InterpreterBinaryNode : public InterpreterNode {
public:
    InterpreterBinaryNode(enum InterpreterNodeType ty, const RamNode* sdw,
            std::unique_ptr<InterpreterNode> lhs, std::unique_ptr<InterpreterNode> rhs,
            RelationHandle* relHandle = nullptr)
            : InterpreterNode(ty, sdw, relHandle), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    /** @brief get left child of node */
    inline const InterpreterNode* getLhs() const {
        return lhs.get();
    }

    /** @brief get right child of node */
    inline const InterpreterNode* getRhs() const {
        return rhs.get();
    }

protected:
    std::unique_ptr<InterpreterNode> lhs;
    std::unique_ptr<InterpreterNode> rhs;
};

/**
 * @class InterpreterSuperInstruction
 * @brief This class encodes information for a super-instruction, which is
 *        used to eliminate Number and TupleElement in index/project/existence operation.
 */
class InterpreterSuperInstruction {
public:
    InterpreterSuperInstruction(size_t i) {
        first.resize(i);
        second.resize(i);
    }

    InterpreterSuperInstruction(const InterpreterSuperInstruction&) = delete;
    InterpreterSuperInstruction& operator=(const InterpreterSuperInstruction&) = delete;
    InterpreterSuperInstruction(InterpreterSuperInstruction&&) = default;

    /** @brief constant value in the lower bound/pattern */
    std::vector<RamDomain> first;
    /** @brief constant value in the upper bound */
    std::vector<RamDomain> second;
    /** @brief Encoded tupleElement expressions in the lower bound/pattern */
    std::vector<std::array<size_t, 3>> tupleFirst;
    /** @brief Encoded tupleElement expressions in the upper bound */
    std::vector<std::array<size_t, 3>> tupleSecond;
    /** @brief Generic expressions in the lower bound/pattern */
    std::vector<std::pair<size_t, std::unique_ptr<InterpreterNode>>> exprFirst;
    /** @brief Generic expressions in the upper bound */
    std::vector<std::pair<size_t, std::unique_ptr<InterpreterNode>>> exprSecond;
};

/**
 * @class InterpreterSuperOperation
 * @brief Interpreter node that utilizes the super instruction optimization should
 *        inherit from this class. E.g. ExistenceCheck, Project
 */
class InterpreterSuperOperation {
public:
    InterpreterSuperOperation(InterpreterSuperInstruction superInst) : superInst(std::move(superInst)) {}

    const InterpreterSuperInstruction& getSuperInst() const {
        return superInst;
    }

protected:
    const InterpreterSuperInstruction superInst;
};

/**
 * @class InterpreterAbstractParallel
 * @brief Interpreter node that utilizes parallel execution should inherit from this class.
 *        Enable node with parallelization preamble.
 */
class InterpreterAbstractParallel {
public:
    /** @brief get preamble */
    inline InterpreterPreamble* getPreamble() const {
        return preamble.get();
    }

    /** @brief set preamble */
    inline void setPreamble(const std::shared_ptr<InterpreterPreamble>& p) {
        preamble = p;
    }

protected:
    std::shared_ptr<InterpreterPreamble> preamble = nullptr;
};

/**
 * @class InterpreterViewOperation
 * @brief Interpreter operation that utilizes the index view from underlying relation should inherit from this
 *        class.
 */
class InterpreterViewOperation {
public:
    InterpreterViewOperation(size_t id) : viewId(id) {}

    inline size_t getViewId() const {
        return viewId;
    }

protected:
    size_t viewId;
};

/**
 * @class InterpreterBinRelOperation
 * @brief Interpreter operation that involves with two relations should inherit from this class.
 *        E.g. Swap, Extend
 */
class InterpreterBinRelOperation {
public:
    InterpreterBinRelOperation(size_t src, size_t target) : src(src), target(target) {}

    inline size_t getSourceId() const {
        return src;
    }

    inline size_t getTargetId() const {
        return target;
    }

protected:
    const size_t src;
    const size_t target;
};

/**
 * @class InterpreterNestedOperation
 * @brief Encode a nested operation for the interpreter node. E.g. Loop, IndexScan
 */
class InterpreterNestedOperation {
public:
    InterpreterNestedOperation(std::unique_ptr<InterpreterNode> nested) : nested(std::move(nested)) {}

    inline const InterpreterNode* getNestedOperation() const {
        return nested.get();
    };

protected:
    std::unique_ptr<InterpreterNode> nested;
};

/**
 * @class InterpreterConditionalOperation
 * @brief Encode a conditional operation for the interpreter node. E.g. Exit, Filter
 */
class InterpreterConditionalOperation {
public:
    InterpreterConditionalOperation(std::unique_ptr<InterpreterNode> cond) : cond(std::move(cond)) {}

    inline const InterpreterNode* getCondition() const {
        return cond.get();
    };

protected:
    std::unique_ptr<InterpreterNode> cond;
};

/**
 * @class InterpreterConstant
 */
class InterpreterConstant : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterTupleElement
 */
class InterpreterTupleElement : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterAutoIncrement
 */
class InterpreterAutoIncrement : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterIntrinsicOperator
 */
class InterpreterIntrinsicOperator : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterUserDefinedOperator
 */
class InterpreterUserDefinedOperator : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterNestedIntrinsicOperator
 */
class InterpreterNestedIntrinsicOperator : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterPackRecord
 */
class InterpreterPackRecord : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterSubroutineArgument
 */
class InterpreterSubroutineArgument : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterTrue
 */
class InterpreterTrue : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterFalse
 */
class InterpreterFalse : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterConjunction
 */
class InterpreterConjunction : public InterpreterBinaryNode {
    using InterpreterBinaryNode::InterpreterBinaryNode;
};

/**
 * @class InterpreterNegation
 */
class InterpreterNegation : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterEmptinessCheck
 */
class InterpreterEmptinessCheck : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterExistenceCheck
 */
class InterpreterExistenceCheck : public InterpreterNode,
                                  public InterpreterSuperOperation,
                                  public InterpreterViewOperation {
public:
    InterpreterExistenceCheck(enum InterpreterNodeType ty, const RamNode* sdw, bool totalSearch,
            size_t viewId, InterpreterSuperInstruction superInst)
            : InterpreterNode(ty, sdw), InterpreterSuperOperation(std::move(superInst)),
              InterpreterViewOperation(viewId), totalSearch(totalSearch) {}

    bool isTotalSearch() const {
        return totalSearch;
    }

private:
    const bool totalSearch;
};

/**
 * @class InterpreterProvenanceExistenceCheck
 */
class InterpreterProvenanceExistenceCheck : public InterpreterUnaryNode,
                                            public InterpreterSuperOperation,
                                            public InterpreterViewOperation {
public:
    InterpreterProvenanceExistenceCheck(enum InterpreterNodeType ty, const RamNode* sdw,
            std::unique_ptr<InterpreterNode> child, size_t viewId, InterpreterSuperInstruction superInst)
            : InterpreterUnaryNode(ty, sdw, std::move(child)),
              InterpreterSuperOperation(std::move(superInst)), InterpreterViewOperation(viewId) {}
};

/**
 * @class InterpreterConstraint
 */
class InterpreterConstraint : public InterpreterBinaryNode {
    using InterpreterBinaryNode::InterpreterBinaryNode;
};

/**
 * @class InterpreterTupleOperation
 */
class InterpreterTupleOperation : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterScan
 */
class InterpreterScan : public InterpreterNode, public InterpreterNestedOperation {
public:
    InterpreterScan(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            std::unique_ptr<InterpreterNode> nested)
            : InterpreterNode(ty, sdw, relHandle), InterpreterNestedOperation(std::move(nested)) {}
};

/**
 * @class InterpreterParallelScan
 */
class InterpreterParallelScan : public InterpreterScan, public InterpreterAbstractParallel {
    using InterpreterScan::InterpreterScan;
};

/**
 * @class InterpreterIndexScan
 */
class InterpreterIndexScan : public InterpreterScan,
                             public InterpreterSuperOperation,
                             public InterpreterViewOperation {
public:
    InterpreterIndexScan(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            std::unique_ptr<InterpreterNode> nested, size_t viewId, InterpreterSuperInstruction superInst)
            : InterpreterScan(ty, sdw, relHandle, std::move(nested)),
              InterpreterSuperOperation(std::move(superInst)), InterpreterViewOperation(viewId) {}
};

/**
 * @class InterpreterParallelIndexScan
 */
class InterpreterParallelIndexScan : public InterpreterIndexScan, public InterpreterAbstractParallel {
public:
    using InterpreterIndexScan::InterpreterIndexScan;
};

/**
 * @class InterpreterChoice
 */
class InterpreterChoice : public InterpreterNode,
                          public InterpreterConditionalOperation,
                          public InterpreterNestedOperation {
public:
    InterpreterChoice(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            std::unique_ptr<InterpreterNode> cond, std::unique_ptr<InterpreterNode> nested)
            : InterpreterNode(ty, sdw, relHandle), InterpreterConditionalOperation(std::move(cond)),
              InterpreterNestedOperation(std::move(nested)) {}
};

/**
 * @class InterpreterParallelChoice
 */
class InterpreterParallelChoice : public InterpreterChoice, public InterpreterAbstractParallel {
    using InterpreterChoice::InterpreterChoice;
};

/**
 * @class InterpreterIndexChoice
 */
class InterpreterIndexChoice : public InterpreterChoice,
                               public InterpreterSuperOperation,
                               public InterpreterViewOperation {
public:
    InterpreterIndexChoice(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            std::unique_ptr<InterpreterNode> cond, std::unique_ptr<InterpreterNode> nested, size_t viewId,
            InterpreterSuperInstruction superInst)
            : InterpreterChoice(ty, sdw, relHandle, std::move(cond), std::move(nested)),
              InterpreterSuperOperation(std::move(superInst)), InterpreterViewOperation(viewId) {}
};

/**
 * @class InterpreterParallelIndexChoice
 */
class InterpreterParallelIndexChoice : public InterpreterIndexChoice, public InterpreterAbstractParallel {
    using InterpreterIndexChoice::InterpreterIndexChoice;
};

/**
 * @class InterpreterUnpackRecord
 */
class InterpreterUnpackRecord : public InterpreterNode, public InterpreterNestedOperation {
public:
    InterpreterUnpackRecord(enum InterpreterNodeType ty, const RamNode* sdw,
            std::unique_ptr<InterpreterNode> expr, std::unique_ptr<InterpreterNode> nested)
            : InterpreterNode(ty, sdw), InterpreterNestedOperation(std::move(nested)), expr(std::move(expr)) {
    }

    inline const InterpreterNode* getExpr() const {
        return expr.get();
    }

protected:
    std::unique_ptr<InterpreterNode> expr;
};

/**
 * @class InterpreterAggregate
 */
class InterpreterAggregate : public InterpreterNode,
                             public InterpreterConditionalOperation,
                             public InterpreterNestedOperation {
public:
    InterpreterAggregate(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            std::unique_ptr<InterpreterNode> expr, std::unique_ptr<InterpreterNode> filter,
            std::unique_ptr<InterpreterNode> nested)
            : InterpreterNode(ty, sdw, relHandle), InterpreterConditionalOperation(std::move(filter)),
              InterpreterNestedOperation(std::move(nested)), expr(std::move(expr)) {}

    inline const InterpreterNode* getExpr() const {
        return expr.get();
    }

protected:
    std::unique_ptr<InterpreterNode> expr;
};

/**
 * @class InterpreterParallelAggregate
 */
class InterpreterParallelAggregate : public InterpreterAggregate, public InterpreterAbstractParallel {
    using InterpreterAggregate::InterpreterAggregate;
};

/**
 * @class InterpreterIndexAggregate
 */
class InterpreterIndexAggregate : public InterpreterAggregate,
                                  public InterpreterSuperOperation,
                                  public InterpreterViewOperation {
public:
    InterpreterIndexAggregate(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            std::unique_ptr<InterpreterNode> expr, std::unique_ptr<InterpreterNode> filter,
            std::unique_ptr<InterpreterNode> nested, size_t viewId, InterpreterSuperInstruction superInst)
            : InterpreterAggregate(ty, sdw, relHandle, std::move(expr), std::move(filter), std::move(nested)),
              InterpreterSuperOperation(std::move(superInst)), InterpreterViewOperation(viewId) {}
};

/**
 * @class InterpreterParallelIndexAggregate
 */
class InterpreterParallelIndexAggregate : public InterpreterIndexAggregate,
                                          public InterpreterAbstractParallel {
    using InterpreterIndexAggregate::InterpreterIndexAggregate;
};

/**
 * @class InterpreterBreak
 */
class InterpreterBreak : public InterpreterNode,
                         public InterpreterConditionalOperation,
                         public InterpreterNestedOperation {
public:
    InterpreterBreak(enum InterpreterNodeType ty, const RamNode* sdw, std::unique_ptr<InterpreterNode> cond,
            std::unique_ptr<InterpreterNode> nested)
            : InterpreterNode(ty, sdw, nullptr), InterpreterConditionalOperation(std::move(cond)),
              InterpreterNestedOperation(std::move(nested)) {}
};

/**
 * @class InterpreterFilter
 */
class InterpreterFilter : public InterpreterNode,
                          public InterpreterConditionalOperation,
                          public InterpreterNestedOperation {
public:
    InterpreterFilter(enum InterpreterNodeType ty, const RamNode* sdw, std::unique_ptr<InterpreterNode> cond,
            std::unique_ptr<InterpreterNode> nested)
            : InterpreterNode(ty, sdw, nullptr), InterpreterConditionalOperation(std::move(cond)),
              InterpreterNestedOperation(std::move(nested)) {}
};

/**
 * @class InterpreterProject
 */
class InterpreterProject : public InterpreterNode, public InterpreterSuperOperation {
public:
    InterpreterProject(enum InterpreterNodeType ty, const RamNode* sdw, RelationHandle* relHandle,
            InterpreterSuperInstruction superInst)
            : InterpreterNode(ty, sdw, relHandle), InterpreterSuperOperation(std::move(superInst)) {}
};

/**
 * @class InterpreterSubroutineReturn
 */
class InterpreterSubroutineReturn : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterSequence
 */
class InterpreterSequence : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterParallel
 */
class InterpreterParallel : public InterpreterCompoundNode {
    using InterpreterCompoundNode::InterpreterCompoundNode;
};

/**
 * @class InterpreterLoop
 */
class InterpreterLoop : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterExit
 */
class InterpreterExit : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterLogRelationTimer
 */
class InterpreterLogRelationTimer : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterLogTimer
 */
class InterpreterLogTimer : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterDebugInfo
 */
class InterpreterDebugInfo : public InterpreterUnaryNode {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterClear
 */
class InterpreterClear : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterCall
 */
class InterpreterCall : public InterpreterNode {
public:
    InterpreterCall(enum InterpreterNodeType ty, const RamNode* sdw, size_t subroutineId)
            : InterpreterNode(ty, sdw), subroutineId(subroutineId) {}

    size_t getSubroutineId() const {
        return subroutineId;
    }

private:
    const size_t subroutineId;
};

/**
 * @class InterpreterLogSize
 */
class InterpreterLogSize : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterIO
 */
class InterpreterIO : public InterpreterNode {
    using InterpreterNode::InterpreterNode;
};

/**
 * @class InterpreterQuery
 */
class InterpreterQuery : public InterpreterUnaryNode, public InterpreterAbstractParallel {
    using InterpreterUnaryNode::InterpreterUnaryNode;
};

/**
 * @class InterpreterExtend
 */
class InterpreterExtend : public InterpreterNode, public InterpreterBinRelOperation {
public:
    InterpreterExtend(enum InterpreterNodeType ty, const RamNode* sdw, size_t src, size_t target)
            : InterpreterNode(ty, sdw), InterpreterBinRelOperation(src, target) {}
};

/**
 * @class InterpreterSwap
 */
class InterpreterSwap : public InterpreterNode, public InterpreterBinRelOperation {
public:
    InterpreterSwap(enum InterpreterNodeType ty, const RamNode* sdw, size_t src, size_t target)
            : InterpreterNode(ty, sdw), InterpreterBinRelOperation(src, target) {}
};

}  // namespace souffle
