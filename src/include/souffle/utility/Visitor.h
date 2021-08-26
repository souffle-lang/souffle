/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Visitor.h
 *
 * Defines a generic visitor pattern for nodes
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <functional>
#include <type_traits>
#include <utility>

namespace souffle {

/** A tag type required for the is_visitor type trait to identify Visitors */
struct visitor_tag {};

template <class Node>
struct visitor_with_type : visitor_tag {};

template <class T>
using is_visitor = std::is_base_of<visitor_tag, T>;

template <typename T>
inline constexpr bool is_visitor_v = is_visitor<T>::value;

/**
 * Extension point for visitors.
 * Can be orderloaded in the namespace of Node
 */
template <class Node>
auto getChildNodes(Node&& node) -> decltype(node.getChildNodes()) {
    return node.getChildNodes();
}

namespace detail {
template <typename T, typename U = void>
struct is_visitable_impl : std::false_type {};

template <typename T>
struct is_visitable_impl<T, std::void_t<decltype(getChildNodes(std::declval<T const&>()))>>
        : is_range<decltype(getChildNodes(std::declval<T&>()))> {};
}  // namespace detail

template <typename T>
using is_visitable = detail::is_visitable_impl<T>;

template <typename T>
inline constexpr bool is_visitable_v = is_visitable<T>::value;

namespace detail {
template <typename T>
using node_base_t = std::remove_const_t<std::remove_pointer_t<
        std::remove_reference_t<decltype(*std::begin(getChildNodes(std::declval<T&>())))>>>;

template <typename T>
constexpr bool ptr_helper_v = souffle::is_pointer_like<remove_cvref_t<T>>;

}  // namespace detail

/**
 * The generic base type of all Visitors realizing the dispatching of
 * visitor calls. Each visitor may define a return type R and a list of
 * extra parameters to be passed along with the visited Nodes to the
 * corresponding visitor function.
 *
 * @tparam R the result type produced by a visit call
 * @tparam Node the type of the node being visited (can be const qualified)
 * @tparam Params extra parameters to be passed to the visit call
 */
template <typename R, class NodeType, typename... Params>
struct Visitor : public visitor_with_type<NodeType> {
    virtual ~Visitor() = default;

    /** The main entry for the user allowing visitors to be utilized as functions */
    R operator()(NodeType& node, Params const&... args) {
        return dispatch(node, args...);
    }

    /**
     * The main entry for a visit process conducting the dispatching of
     * a visit to the various sub-types of Nodes. Sub-classes may override
     * this implementation to conduct pre-visit operations.
     *
     * @param node the node to be visited
     * @param args a list of extra parameters to be forwarded
     */
    virtual R dispatch(NodeType& node, Params const&... args) = 0;

    /** The base case for all visitors -- if no more specific overload was defined */
    virtual R visit_(type_identity<std::remove_const_t<NodeType>>, const NodeType& /*node*/,
            Params const&... /*args*/) {
        return R();
    }
};

#define SOUFFLE_VISITOR_FORWARD(Kind) \
    if (auto* n = as<Kind>(node)) return visit_(type_identity<Kind>(), *n, args...);

#define SOUFFLE_VISITOR_LINK(Kind, Parent)                                                        \
    virtual R visit_(type_identity<Kind>, copy_const<NodeType, Kind>& n, Params const&... args) { \
        return visit_(type_identity<Parent>(), n, args...);                                       \
    }

template <class R, class Visitor, typename... Args>
std::enable_if_t<is_range_v<remove_cvref_t<R>> && is_visitor_v<Visitor>> visit(
        R&& range, Visitor& visitor, Args const&... args);

/**
 * A utility function visiting all nodes within the given root
 * recursively in a depth-first pre-order fashion, applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the structure to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <class Node, class Visitor, typename... Args>
std::enable_if_t<is_visitable_v<Node> && is_visitor_v<Visitor>> visit(
        Node&& root, Visitor& visitor, Args const&... args) {
    visitor(root, args...);
    souffle::visit(getChildNodes(root), visitor, args...);
}

/**
 * A utility function visiting all nodes within a given container of root nodes
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param list the list of roots of the ASTs to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename R, typename Visitor, typename... Args>
std::enable_if_t<is_range_v<remove_cvref_t<R>> && is_visitor_v<Visitor>> visit(
        R&& range, Visitor& visitor, Args const&... args) {
    for (auto&& cur : range) {
        // FIXME: Remove this once nodes are converted to references
        if constexpr (detail::ptr_helper_v<decltype(cur)>) {
            if (cur != nullptr) {
                souffle::visit(*cur, visitor, args...);
            }
        } else {
            souffle::visit(cur, visitor, args...);
        }
    }
}

namespace detail {

/**
 * A specialized visitor wrapping a lambda function -- an auxiliary type required
 * for visitor convenience functions.
 */
template <class NodeToVisit, class Node, typename F>
struct LambdaVisitor : public Visitor<void, copy_const<NodeToVisit, Node>> {
    F lambda;
    LambdaVisitor(F lam) : lambda(std::move(lam)) {}
    void dispatch(copy_const<NodeToVisit, Node>& node) override {
        // Don't use as<> to allow cross-casting to mixins
        if (auto* n = dynamic_cast<NodeToVisit*>(&node)) {
            lambda(*n);
        }
    }
};

// Only used for deduction
template <typename R, typename T>
T& getNodeHelper(std::function<R(T&)>);

template <typename F>
typename lambda_traits<F>::template arg<0>& getNodeHelper(F const&);

/**
 * A factory function for creating LambdaVisitor instances.
 */
template <class Node, typename F>
auto makeLambdaVisitor(F&& f) {
    using NodeToVisit = std::remove_reference_t<decltype(getNodeHelper(f))>;
    using NodeBase = detail::node_base_t<Node>;
    return LambdaVisitor<NodeToVisit, copy_const<NodeToVisit, NodeBase>, std::remove_reference_t<F>>(
            std::forward<F>(f));
}
}  // namespace detail

/**
 * A utility function visiting all nodes within the given root
 * recursively in a depth-first pre-order fashion, applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the structure to be visited
 * @param visitor the visitor to be applied on each node
 */
template <class Node, typename F>
std::enable_if_t<is_visitable_v<Node> && !is_visitor_v<F>> visit(Node&& root, F&& fun) {
    auto visitor = detail::makeLambdaVisitor<std::remove_reference_t<Node>>(std::forward<F>(fun));
    souffle::visit(root, visitor);
}

/**
 * A utility function visiting all nodes within a given container of root nodes
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param list the list of roots of the ASTs to be visited
 * @param fun the function to be applied
 */
template <typename R, typename F>
std::enable_if_t<is_range_v<R> && !is_visitor_v<remove_cvref_t<F>>> visit(R const& range, F&& fun) {
    using Elem = remove_cvref_t<decltype(*std::begin(range))>;
    using Node = std::conditional_t<is_pointer_like<Elem>, decltype(*std::declval<Elem&>()), Elem>;
    auto visitor = detail::makeLambdaVisitor<std::remove_reference_t<Node>>(std::forward<F>(fun));
    souffle::visit(range, visitor);
}

}  // namespace souffle
