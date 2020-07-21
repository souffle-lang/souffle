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

#include "SrcLocation.h"
#include "utility/ContainerUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle {

class AstNodeMapper;

/**
 *  @class AstNode
 *  @brief Abstract class for syntactic elements in a Datalog program.
 */
class AstNode {
public:
    AstNode(SrcLocation loc = {}) : location(std::move(loc)){};
    virtual ~AstNode() = default;

    /** Return source location of the AstNode */
    const SrcLocation& getSrcLoc() const {
        return location;
    }

    /** Set source location for the AstNode */
    void setSrcLoc(SrcLocation l) {
        location = std::move(l);
    }

    /** Return source location of the syntactic element */
    std::string extloc() const {
        return location.extloc();
    }

    /** Equivalence check for two AST nodes */
    bool operator==(const AstNode& other) const {
        if (this == &other) {
            return true;
        } else if (typeid(*this) == typeid(*&other)) {
            return equal(other);
        }
        return false;
    }

    /** Inequality check for two AST nodes */
    bool operator!=(const AstNode& other) const {
        return !(*this == other);
    }

    /** Create a clone (i.e. deep copy) of this node */
    virtual AstNode* clone() const = 0;

    /** Apply the mapper to all child nodes */
    virtual void apply(const AstNodeMapper& /* mapper */) {}

    /** Obtain a list of all embedded AST child nodes */
    virtual std::vector<const AstNode*> getChildNodes() const {
        return {};
    }

    /** Print node onto an output stream */
    friend std::ostream& operator<<(std::ostream& out, const AstNode& node) {
        node.print(out);
        return out;
    }

protected:
    /** Output to a given output stream */
    virtual void print(std::ostream& os) const = 0;

    /** Abstract equality check for two AST nodes */
    virtual bool equal(const AstNode& /* other */) const {
        return true;
    }

private:
    /** Source location of a syntactic element */
    SrcLocation location;
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
    virtual Own<AstNode> operator()(Own<AstNode> node) const = 0;

    /**
     * Wrapper for any subclass of the AST node hierarchy performing type casts.
     */
    template <typename T>
    Own<T> operator()(Own<T> node) const {
        Own<AstNode> resPtr = (*this)(Own<AstNode>(static_cast<AstNode*>(node.release())));
        assert(nullptr != dynamic_cast<T*>(resPtr.get()) && "Invalid target node!");
        return Own<T>(dynamic_cast<T*>(resPtr.release()));
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

    Own<AstNode> operator()(Own<AstNode> node) const override {
        return lambda(std::move(node));
    }
};
}  // namespace detail

/**
 * Creates a node mapper based on a corresponding lambda expression.
 */
template <typename Lambda>
detail::LambdaNodeMapper<Lambda> makeLambdaAstMapper(const Lambda& lambda) {
    return detail::LambdaNodeMapper<Lambda>(lambda);
}

}  // end of namespace souffle
