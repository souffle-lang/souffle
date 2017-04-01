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

#include "RamCondition.h"
#include "RamOperation.h"
#include "RamStatement.h"
#include "RamValue.h"

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
     * @param node the node to be visited
     * @param args a list of extra parameters to be forwarded
     */
    virtual R visit(const RamNode& node, Params... args) {
        // dispatch node processing based on dynamic type

        switch (node.getNodeType()) {
#define FORWARD(Kind) \
    case (RN_##Kind): \
        return visit##Kind(static_cast<const Ram##Kind&>(node), args...);

            // values
            FORWARD(ElementAccess);
            FORWARD(Number);
            FORWARD(UnaryOperator);
            FORWARD(BinaryOperator);
            FORWARD(TernaryOperator);
            FORWARD(AutoIncrement);
            FORWARD(Pack);

            // conditions
            FORWARD(Empty);
            FORWARD(NotExists);
            FORWARD(And);
            FORWARD(BinaryRelation);

            // operations
            FORWARD(Project);
            FORWARD(Lookup);
            FORWARD(Scan);
            FORWARD(Aggregate);

            // statements
            FORWARD(Create);
            FORWARD(Fact);
            FORWARD(Load);
            FORWARD(Store);
            FORWARD(Insert);
            FORWARD(Clear);
            FORWARD(Drop);
            FORWARD(PrintSize);
            FORWARD(LogSize);

            FORWARD(Merge);
            FORWARD(Swap);

            // control flow
            FORWARD(Sequence);
            FORWARD(Loop);
            FORWARD(Parallel);
            FORWARD(Exit);
            FORWARD(LogTimer);
            FORWARD(DebugInfo);

#undef FORWARD
        }

        // did not work ...

        std::cerr << "Unsupported type: " << typeid(node).name() << "\n";
        assert(false && "Missing RAM Node Category!");
        return R();
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
    LINK(Create, RelationStatement);
    LINK(Fact, RelationStatement);
    LINK(Load, RelationStatement);
    LINK(Store, RelationStatement);
    LINK(Insert, Statement);
    LINK(Clear, RelationStatement);
    LINK(Drop, RelationStatement);
    LINK(PrintSize, RelationStatement);
    LINK(LogSize, RelationStatement);

    LINK(RelationStatement, Statement);

    LINK(Merge, Statement);
    LINK(Swap, Statement);

    LINK(Sequence, Statement);
    LINK(Loop, Statement);
    LINK(Parallel, Statement);
    LINK(Exit, Statement);
    LINK(LogTimer, Statement);
    LINK(DebugInfo, Statement);

    LINK(Statement, Node);

    // -- operations --
    LINK(Project, Operation)
    LINK(Lookup, Search)
    LINK(Scan, Search)
    LINK(Aggregate, Search)
    LINK(Search, Operation)

    LINK(Operation, Node)

    // -- conditions --
    LINK(And, Condition)
    LINK(BinaryRelation, Condition)
    LINK(NotExists, Condition)
    LINK(Empty, Condition)

    LINK(Condition, Node)

    // -- values --
    LINK(Number, Value)
    LINK(ElementAccess, Value)
    LINK(UnaryOperator, Value)
    LINK(BinaryOperator, Value)
    LINK(TernaryOperator, Value)
    LINK(AutoIncrement, Value)
    LINK(Pack, Value)

    LINK(Value, Node)

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
        if (cur) {
            visitDepthFirstPreOrder(*cur, visitor, args...);
        }
    }
}

/**
 * A utility function visiting all nodes within the RAM fragment rooted by the given node
 * recursively in a depth-first post-order fashion applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the RAM fragment to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename R, typename... Ps, typename... Args>
void visitDepthFirstPostOrder(const RamNode& root, RamVisitor<R, Ps...>& visitor, Args&... args) {
    for (const RamNode* cur : root.getChildNodes()) {
        if (cur) {
            visitDepthFirstPreOrder(*cur, visitor, args...);
        }
    }
    visitor(root, args...);
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
    LambdaRamVisitor(const std::function<R(const N&)>& lambda) : lambda(lambda) {}
    void visit(const RamNode& node) override {
        if (const N* n = dynamic_cast<const N*>(&node)) {
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
    enum { value = std::is_base_of<ram_visitor_tag, T>::value };
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
