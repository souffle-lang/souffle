/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */
/************************************************************************
 *
 * @file AstNode.h
 *
 * Top level syntactic element of intermediate representation,
 * i.e., a node of abstract syntax tree
 *
 ***********************************************************************/

#pragma once

#include "AstSrcLocation.h"
#include "AstTypes.h"
#include "Util.h"

#include <limits>
#include <memory>
#include <typeinfo>

#include <libgen.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

namespace souffle {

// forward declaration -- see below
class AstNodeMapper;

/**
 *  @class AstNode
 *  @brief AstNode is a superclass for all elements of IR that
 *         correspond to syntactic elements of a datalog program.
 */
class AstNode {
    /** Source location of a syntactic element */
    AstSrcLocation location;

public:
    virtual ~AstNode() = default;

    /** Return source location of the AstNode */
    AstSrcLocation getSrcLoc() const {
        return location;
    }

    /** Set source location for the AstNode */
    void setSrcLoc(const AstSrcLocation& l) {
        location = l;
    }

    /** Return extended location associated with this
      AstNode (redirect from SrcLoc). */
    std::string extloc() const {
        return location.extloc();
    }

    /** Enables the comparison of AST nodes */
    bool operator==(const AstNode& other) const {
        return this == &other || (typeid(*this) == typeid(other) && equal(other));
    }

    /** Enables the comparison of AST nodes */
    bool operator!=(const AstNode& other) const {
        return !(*this == other);
    }

    /** Requests an independent, deep copy of this node */
    virtual AstNode* clone() const = 0;

    /** Applies the node mapper to all child nodes and conducts the corresponding replacements */
    virtual void apply(const AstNodeMapper& mapper) = 0;

    /** Obtains a list of all embedded child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const = 0;

    /** Output to a given output stream */
    virtual void print(std::ostream& os) const = 0;

    /** Enables all nodes to be printed to some output stream */
    friend std::ostream& operator<<(std::ostream& out, const AstNode& node) {
        node.print(out);
        return out;
    }

protected:
    /** An internal function to determine equality to another node */
    virtual bool equal(const AstNode& other) const = 0;
};

/**
 * An abstract base class for AST node manipulation operations mapping
 * a AST nodes to substitutions.
 */
class AstNodeMapper {
public:
    /** A virtual destructor for this abstract type */
    virtual ~AstNodeMapper() = default;

    /**
     * Computes a replacement for the given node. If the given nodes
     * is to be replaced, the handed in node will be destroyed by the mapper
     * and the returned node will become owned by the caller.
     */
    virtual std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const = 0;

    /**
     * A generic wrapper over the map function above to avoid unnecessary
     * casting operations.
     */
    template <typename T>
    std::unique_ptr<T> operator()(std::unique_ptr<T> node) const {
        std::unique_ptr<AstNode> resPtr =
                (*this)(std::unique_ptr<AstNode>(static_cast<AstNode*>(node.release())));
        assert(dynamic_cast<T*>(resPtr.get()) && "Invalid target node!");
        return std::unique_ptr<T>(dynamic_cast<T*>(resPtr.release()));
    }
};

namespace detail {

/**
 * A special AstNodeMapper wrapping a lambda conducting node transformations.
 */
template <typename Lambda>
class LambdaNodeMapper : public AstNodeMapper {
    const Lambda& lambda;

public:
    LambdaNodeMapper(const Lambda& lambda) : lambda(lambda) {}

    std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
        return lambda(std::move(node));
    }
};
}  // namespace detail

/**
 * Creates a node mapper based on a corresponding lambda expression.
 */
template <typename Lambda>
detail::LambdaNodeMapper<Lambda> makeLambdaMapper(const Lambda& lambda) {
    return detail::LambdaNodeMapper<Lambda>(lambda);
}

}  // end of namespace souffle
