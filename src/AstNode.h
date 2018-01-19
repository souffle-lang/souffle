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
 * Abstract class definitions for AST nodes
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

class AstNodeMapper;

/**
 *  @class AstNode
 *  @brief Abstract class for syntactic elements in a Datalog program.
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

    /** Return source location of the syntactic element */
    std::string extloc() const {
        return location.extloc();
    }

    /** Equivalence check for two AST nodes */
    bool operator==(const AstNode& other) const {
        return this == &other || (typeid(*this) == typeid(other) && equal(other));
    }

    /** Inequality check for two AST nodes */
    bool operator!=(const AstNode& other) const {
        return !(*this == other);
    }

    /** Create a clone (i.e. deep copy) of this node */
    virtual AstNode* clone() const = 0;

    /** Apply the mapper to all child nodes */
    virtual void apply(const AstNodeMapper& mapper) = 0;

    /** Obtain a list of all embedded AST child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const = 0;

    /** Output to a given output stream */
    virtual void print(std::ostream& os) const = 0;

    /** Print node onto an output stream */
    friend std::ostream& operator<<(std::ostream& out, const AstNode& node) {
        node.print(out);
        return out;
    }

protected:
    /** Abstract equality check for two AST nodes */
    virtual bool equal(const AstNode& other) const = 0;
};

/**
 * An abstract class for manipulating AST Nodes by substitution
 */
class AstNodeMapper {
public:
    virtual ~AstNodeMapper() = default;

    /**
     * Abstract replacement method for a node.
     *
     * If the given nodes is to be replaced, the handed in node
     * will be destroyed by the mapper and the returned node
     * will become owned by the caller.
     */
    virtual std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const = 0;

    /**
     * Wrapper for any subclass of the AST node hierarchy performing type casts.
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
