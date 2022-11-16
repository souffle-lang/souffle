/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Node.h
 *
 * Declaration of RAM node
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/Types.h"
#include "souffle/utility/VisitorFwd.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram {

class Node;
using NodeMapper = souffle::detail::NodeMapper<Node>;

namespace detail {
// Seems the gcc in Jenkins is not happy with the inline lambdas
struct RefCaster {
    auto operator()(Node const* node) const -> Node const& {
        return *node;
    }
};

struct ConstCaster {
    auto operator()(Node const* node) const -> Node& {
        return *const_cast<Node*>(node);
    }
};
}  // namespace detail

/**
 *  @class Node
 *  @brief Node is a superclass for all RAM IR classes.
 */
class Node {
protected:
    using NodeVec = std::vector<Node const*>;  // std::reference_wrapper<Node const>>;

public:
    /// LLVM-style RTTI
    ///
    /// Each class under the ram::Node hierarchy must appear here and must implement
    /// `static bool classof(const Node*)`.
    ///
    /// When class T is final, we must provide a single enum:
    ///
    ///   ...
    ///   NK_T,
    ///   ...
    ///
    /// When class T is non-final, we must provide enums like this:
    ///
    ///   NK_T,
    ///     NK_Child1,
    ///     ...
    ///     NK_ChildN,
    ///   NK_LastT
    ///
    ///
    // clang-format off
    enum NodeKind {
        NK_NONE,
        NK_Condition,
            NK_AbstractExistenceCheck, //Abstract Class
                NK_ExistenceCheck,
                NK_ProvenanceExistenceCheck,
            NK_LastAbstractExistenceCheck,

            NK_Conjunction,
            NK_Constraint,
            NK_EmptinessCheck,
            NK_False,
            NK_Negation,
            NK_True,
        NK_LastCondition,

        NK_Expression,
            NK_AbstractOperator,
                NK_IntrinsicOperator,
                NK_UserDefinedOperator,
            NK_LastAbstractOperator,

            NK_AutoIncrement,
            NK_NumericConstant,
                NK_FloatConstant,
                NK_SignedConstant,
                NK_UnsignedConstant,
            NK_LastNumericConstant,

            NK_PackRecord,
            NK_RelationSize,
            NK_SubroutineArgument,
            NK_StringConstant,
            NK_TupleElement,
            NK_UndefValue,
            NK_Variable,
        NK_LastExpression,

        NK_Operation,
            NK_Erase,
            NK_Insert,
                NK_GuardedInsert,
            NK_LastInsert,

            NK_NestedOperation,
                NK_AbstractConditional,
                    NK_Break,
                    NK_Filter,
                NK_LastAbstractConditional,

                NK_TupleOperation,
                    NK_RelationOperation,
                        NK_Aggregate,
                            NK_ParallelAggregate,
                        NK_LastAggregate,

                        NK_IfExists,
                            NK_ParallelIfExists,
                        NK_LastIfExists,

                        NK_IndexOperation,
                            NK_IndexAggregate,
                                NK_ParallelIndexAggregate,
                            NK_LastIndexAggregate,

                            NK_IndexIfExists,
                                NK_ParallelIndexIfExists,
                            NK_LastIndexIfExists,

                            NK_IndexScan,
                                NK_ParallelIndexScan,
                            NK_LastIndexScan,
                        NK_LastIndexOperation,

                        NK_Scan,
                            NK_ParallelScan,
                        NK_LastScan,

                    NK_LastRelationOperation,

                    NK_UnpackRecord,
                    NK_NestedIntrinsicOperator,
                NK_LastTupleOperation,

            NK_LastNestedOperation,

            NK_Project,
            NK_SubroutineReturn,
        NK_LastOperation,

        NK_Program,
        NK_Relation,
        NK_Statement,
            NK_Assign,

            NK_BinRelationStatement,
                NK_MergeExtend,
                NK_Swap,
            NK_LastBinRelationStatement,

            NK_Call,
            NK_DebugInfo,
            NK_Exit,
            NK_ListStatement,
                NK_Parallel,
                NK_Sequence,
            NK_LastListStatement,

            NK_LogTimer,
            NK_Loop,
            NK_Query,
            NK_RelationStatement,
                NK_Clear,
                NK_EstimateJoinSize,
                NK_IO,
                NK_LogRelationTimer,
                NK_LogSize,
            NK_LastRelationStatement,

        NK_LastStatement,
    };
    // clang-format on
private:
    const NodeKind Kind;

public:
    explicit Node(NodeKind K);
    Node() = delete;
    virtual ~Node() = default;
    Node(Node const&) = delete;
    Node& operator=(Node const&) = delete;

    NodeKind getKind() const;

    /**
     * @brief Equivalence check for two RAM nodes
     */
    bool operator==(const Node& other) const {
        return this == &other || (typeid(*this) == typeid(other) && equal(other));
    }

    /**
     * @brief Inequality check for two RAM nodes
     */
    bool operator!=(const Node& other) const {
        return !(*this == other);
    }

    /** @brief Create a clone (i.e. deep copy) of this node as a smart-pointer */
    Own<Node> cloneImpl() const {
        return Own<Node>(cloning());
    }

    /**
     * @brief Apply the mapper to all child nodes
     */
    virtual void apply(const NodeMapper&) {}

    /**
     * @brief Rewrite a child node
     */
    virtual void rewrite(const Node* oldNode, Own<Node> newNode);

    /**
     * @brief Obtain list of all embedded child nodes
     */
    using ConstChildNodes = OwningTransformRange<NodeVec, detail::RefCaster>;
    ConstChildNodes getChildNodes() const;

    /*
     * Using the ConstCastRange saves the user from having to write
     * getChildNodes() and getChildNodes() const
     */
    using ChildNodes = OwningTransformRange<NodeVec, detail::ConstCaster>;
    ChildNodes getChildNodes();

    /**
     * Print RAM on a stream
     */
    friend std::ostream& operator<<(std::ostream& out, const Node& node) {
        node.print(out);
        return out;
    }

protected:
    /**
     * @brief Print RAM node
     */
    virtual void print(std::ostream& out = std::cout) const = 0;

    /**
     * @brief Equality check for two RAM nodes.
     * Default action is that nothing needs to be checked.
     */
    virtual bool equal(const Node&) const {
        return true;
    }

    /**
     * @brief Create a cloning (i.e. deep copy) of this node
     */
    virtual Node* cloning() const = 0;

    virtual NodeVec getChildren() const {
        return {};
    }
};

}  // namespace souffle::ram

SOUFFLE_DECLARE_VISITABLE_ROOT_TYPE(::souffle::ram::Node)
