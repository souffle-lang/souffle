/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamVisitor.h
 *
 * Provides some infrastructure for the implementation of operations
 * on top of RAM structures.
 *
 ***********************************************************************/

#pragma once

#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamProgram.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include "utility/FunctionalUtil.h"
#include "utility/MiscUtil.h"
#include <cstddef>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace souffle {

/** A tag type required for the is_ram_visitor type trait to identify RamVisitors */
struct ram_visitor_tag {};

/**
 * The generic base type of all RamVisitors realizing the dispatching of
 * visitor calls. Each visitor may define a return type R and a list of
 * extra parameters to be passed along with the visited RamNodes to the
 * corresponding visitor function.
 *
 * @tparam R the result type produced by a visit call
 * @tparam Params extra parameters to be passed to the visit call
 */
template <typename R = void, typename... Params>
struct RamVisitor : public ram_visitor_tag {
    /** A virtual destructor */
    virtual ~RamVisitor() = default;

    /** The main entry for the user allowing visitors to be utilized as functions */
    R operator()(const RamNode& node, Params... args) {
        return visit(node, args...);
    }

    /** The main entry for the user allowing visitors to be utilized as functions */
    R operator()(const RamNode* node, Params... args) {
        return visit(*node, args...);
    }

    /**
     * The main entry for a visit process conducting the dispatching of
     * a visit to the various sub-types of RamNodes. Sub-classes may override
     * this implementation to conduct pre-visit operations.
     *
     * Note that the order of this list is important. Sub-classes must be listed
     * before their super-classes; otherwise sub-classes cannot be visited.
     *
     * @param node the node to be visited
     * @param args a list of extra parameters to be forwarded
     */
    virtual R visit(const RamNode& node, Params... args) {
        // dispatch node processing based on dynamic type

#define FORWARD(Kind) \
    if (const auto* n = dynamic_cast<const Ram##Kind*>(&node)) return visit##Kind(*n, args...);

        // Relation
        FORWARD(Relation);
        FORWARD(RelationReference);

        // Expressions
        FORWARD(TupleElement);
        FORWARD(SignedConstant);
        FORWARD(UnsignedConstant);
        FORWARD(FloatConstant);
        FORWARD(Constant);
        FORWARD(IntrinsicOperator);
        FORWARD(UserDefinedOperator);
        FORWARD(AutoIncrement);
        FORWARD(PackRecord);
        FORWARD(SubroutineArgument);
        FORWARD(UndefValue);

        // Conditions
        FORWARD(True);
        FORWARD(False);
        FORWARD(EmptinessCheck);
        FORWARD(ExistenceCheck);
        FORWARD(ProvenanceExistenceCheck);
        FORWARD(Conjunction);
        FORWARD(Negation);
        FORWARD(Constraint);

        // Operations
        FORWARD(Filter);
        FORWARD(Break);
        FORWARD(Project);
        FORWARD(SubroutineReturn);
        FORWARD(UnpackRecord);
        FORWARD(NestedIntrinsicOperator);
        FORWARD(ParallelScan);
        FORWARD(Scan);
        FORWARD(ParallelIndexScan);
        FORWARD(IndexScan);
        FORWARD(ParallelChoice);
        FORWARD(Choice);
        FORWARD(ParallelIndexChoice);
        FORWARD(IndexChoice);
        FORWARD(Aggregate);
        FORWARD(IndexAggregate);

        // Statements
        FORWARD(IO);
        FORWARD(Query);
        FORWARD(Clear);
        FORWARD(LogSize);

        FORWARD(Swap);
        FORWARD(Extend);

        // Control-flow
        FORWARD(Program);
        FORWARD(Sequence);
        FORWARD(Loop);
        FORWARD(Parallel);
        FORWARD(Exit);
        FORWARD(LogTimer);
        FORWARD(LogRelationTimer);
        FORWARD(DebugInfo);
        FORWARD(Call);

#undef FORWARD

        // did not work ...
        fatal("unsupported type: %s", typeid(node).name());
    }

    virtual R visit(const RamNode* node, Params... args) {
        return visit(*node, args...);
    }

protected:
#define LINK(Node, Parent)                                      \
    virtual R visit##Node(const Ram##Node& n, Params... args) { \
        return visit##Parent(n, args...);                       \
    }

    // -- statements --
    LINK(IO, RelationStatement);
    LINK(Query, Statement);
    LINK(Clear, RelationStatement);
    LINK(LogSize, RelationStatement);

    LINK(RelationStatement, Statement);

    LINK(Swap, BinRelationStatement);
    LINK(Extend, BinRelationStatement);
    LINK(BinRelationStatement, Statement);

    LINK(Sequence, ListStatement);
    LINK(Loop, Statement);
    LINK(Parallel, ListStatement);
    LINK(ListStatement, Statement);
    LINK(Exit, Statement);
    LINK(LogTimer, Statement);
    LINK(LogRelationTimer, Statement);
    LINK(DebugInfo, Statement);
    LINK(Call, Statement);

    LINK(Statement, Node);

    // -- operations --
    LINK(Project, Operation);
    LINK(SubroutineReturn, Operation);
    LINK(UnpackRecord, TupleOperation);
    LINK(NestedIntrinsicOperator, TupleOperation)
    LINK(Scan, RelationOperation);
    LINK(ParallelScan, Scan);
    LINK(IndexScan, IndexOperation);
    LINK(ParallelIndexScan, IndexScan);
    LINK(Choice, RelationOperation);
    LINK(ParallelChoice, Choice);
    LINK(IndexChoice, IndexOperation);
    LINK(ParallelIndexChoice, IndexChoice);
    LINK(RelationOperation, TupleOperation);
    LINK(Aggregate, RelationOperation);
    LINK(IndexAggregate, IndexOperation);
    LINK(IndexOperation, RelationOperation);
    LINK(TupleOperation, NestedOperation);
    LINK(Filter, AbstractConditional);
    LINK(Break, AbstractConditional);
    LINK(AbstractConditional, NestedOperation);
    LINK(NestedOperation, Operation);

    LINK(Operation, Node);

    // -- conditions --
    LINK(True, Condition);
    LINK(False, Condition);
    LINK(Conjunction, Condition);
    LINK(Negation, Condition);
    LINK(Constraint, Condition);
    LINK(ExistenceCheck, AbstractExistenceCheck);
    LINK(ProvenanceExistenceCheck, AbstractExistenceCheck);
    LINK(EmptinessCheck, Condition);
    LINK(AbstractExistenceCheck, Condition);

    LINK(Condition, Node);

    // -- values --
    LINK(SignedConstant, Constant);
    LINK(UnsignedConstant, Constant);
    LINK(FloatConstant, Constant);
    LINK(Constant, Expression);
    LINK(UndefValue, Expression);
    LINK(TupleElement, Expression);
    LINK(IntrinsicOperator, AbstractOperator);
    LINK(UserDefinedOperator, AbstractOperator);
    LINK(AbstractOperator, Expression);
    LINK(AutoIncrement, Expression);
    LINK(PackRecord, Expression);
    LINK(SubroutineArgument, Expression);

    LINK(Expression, Node);

    // -- program --
    LINK(Program, Node);

    // -- relation
    LINK(Relation, Node);
    LINK(RelationReference, Node);

#undef LINK

    /** The base case for all visitors -- if no more specific overload was defined */
    virtual R visitNode(const RamNode& /*node*/, Params... /*args*/) {
        return R();
    }
};

/**
 * A utility function visiting all nodes within the RAM fragment rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the RAM fragment to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename R, typename... Ps, typename... Args>
void visitDepthFirstPreOrder(const RamNode& root, RamVisitor<R, Ps...>& visitor, Args&... args) {
    visitor(root, args...);
    for (const RamNode* cur : root.getChildNodes()) {
        if (cur != nullptr) {
            visitDepthFirstPreOrder(*cur, visitor, args...);
        }
    }
}

/**
 * A utility function visiting all nodes within the RAM fragments rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the RAM fragments to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename R, typename... Ps, typename... Args>
void visitDepthFirst(const RamNode& root, RamVisitor<R, Ps...>& visitor, Args&... args) {
    visitDepthFirstPreOrder(root, visitor, args...);
}

namespace detail {

/**
 * A specialized visitor wrapping a lambda function -- an auxiliary type required
 * for visitor convenience functions.
 */
template <typename R, typename N>
struct LambdaRamVisitor : public RamVisitor<void> {
    std::function<R(const N&)> lambda;
    LambdaRamVisitor(std::function<R(const N&)> lambda) : lambda(std::move(lambda)) {}
    void visit(const RamNode& node) override {
        if (const auto* n = dynamic_cast<const N*>(&node)) {
            lambda(*n);
        }
    }
};

/**
 * A factory function for creating LambdaRamVisitor instances.
 */
template <typename R, typename N>
LambdaRamVisitor<R, N> makeLambdaRamVisitor(const std::function<R(const N&)>& fun) {
    return LambdaRamVisitor<R, N>(fun);
}

/**
 * A type trait determining whether a given type is a visitor or not.
 */
template <typename T>
struct is_ram_visitor {
    static constexpr size_t value = std::is_base_of<ram_visitor_tag, T>::value;
};

template <typename T>
struct is_ram_visitor<const T> : public is_ram_visitor<T> {};

template <typename T>
struct is_ram_visitor<T&> : public is_ram_visitor<T> {};
}  // namespace detail

/**
 * A utility function visiting all nodes within the RAM fragment rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param root the root of the RAM fragment to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename R, typename N>
void visitDepthFirst(const RamNode& root, const std::function<R(const N&)>& fun) {
    auto visitor = detail::makeLambdaRamVisitor(fun);
    visitDepthFirst<void>(root, visitor);
}

/**
 * A utility function visiting all nodes within the RAM fragment rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param root the root of the RAM fragment to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename Lambda, typename R = typename lambda_traits<Lambda>::result_type,
        typename N = typename lambda_traits<Lambda>::arg0_type>
typename std::enable_if<!detail::is_ram_visitor<Lambda>::value, void>::type visitDepthFirst(
        const RamNode& root, const Lambda& fun) {
    visitDepthFirst(root, std::function<R(const N&)>(fun));
}

}  // end of namespace souffle
