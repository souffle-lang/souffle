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
#include <typeinfo>
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
    Node() = default;
    Node(Node const&) = delete;
    virtual ~Node() = default;
    Node& operator=(Node const&) = delete;

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
        node.print_sexpr(out);
        // node.print(out);
        return out;
    }

protected:
    /**
     * @brief Print RAM node
     */
    virtual void print(std::ostream& out = std::cout) const = 0;

    /**
     * @brief Print RAM node as  sexpr 
     */
    virtual void print_sexpr(std::ostream& out = std::cout) const = 0;

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
