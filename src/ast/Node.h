/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Node.h
 *
 * Defines the AST abstract node class
 *
 ***********************************************************************/

#pragma once

#include "parser/SrcLocation.h"
#include "souffle/utility/Iteration.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/Types.h"
#include "souffle/utility/VisitorFwd.h"
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace souffle::ast {

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
 *  @brief Abstract class for syntactic elements in an input program.
 */
class Node {
public:
    Node(SrcLocation loc = {});
    virtual ~Node() = default;
    // Make sure we don't accidentally copy/slice
    Node(Node const&) = delete;
    Node& operator=(Node const&) = delete;

    /** Return source location of the Node */
    const SrcLocation& getSrcLoc() const {
        return location;
    }

    /** Set source location for the Node */
    void setSrcLoc(SrcLocation l);

    /** Return source location of the syntactic element */
    std::string extloc() const {
        return location.extloc();
    }

    /** Equivalence check for two AST nodes */
    bool operator==(const Node& other) const;

    /** Inequality check for two AST nodes */
    bool operator!=(const Node& other) const {
        return !(*this == other);
    }

    /** Create a clone (i.e. deep copy) of this node */
    Own<Node> cloneImpl() const;

    /** Apply the mapper to all child nodes */
    virtual void apply(const NodeMapper& /* mapper */);

    using NodeVec = std::vector<Node const*>;  // std::reference_wrapper<Node const>>;

    using ConstChildNodes = OwningTransformRange<NodeVec, detail::RefCaster>;
    /** Obtain a list of all embedded AST child nodes */
    ConstChildNodes getChildNodes() const;

    /*
     * Using the ConstCastRange saves the user from having to write
     * getChildNodes() and getChildNodes() const
     */
    using ChildNodes = OwningTransformRange<NodeVec, detail::ConstCaster>;
    ChildNodes getChildNodes();

    /** Print node onto an output stream */
    friend std::ostream& operator<<(std::ostream& out, const Node& node);

protected:
    /** Output to a given output stream */
    virtual void print(std::ostream& os) const = 0;

    virtual NodeVec getChildren() const;

private:
    /** Abstract equality check for two AST nodes */
    virtual bool equal(const Node& /* other */) const;

    virtual Node* cloning() const = 0;

private:
    /** Source location of a syntactic element */
    SrcLocation location;
};

}  // namespace souffle::ast

SOUFFLE_DECLARE_VISITABLE_ROOT_TYPE(::souffle::ast::Node)
