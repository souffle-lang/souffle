/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamNode.h
 *
 * Top level syntactic element of intermediate representation,
 * i.e., a node of the RAM machine code.
 *
 ***********************************************************************/

#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <vector>

namespace souffle {

class RamNode;

/**
 * @class RamNodeMapper
 * @brief An abstract class for manipulating RAM Nodes by substitution
 */
class RamNodeMapper {
public:
    virtual ~RamNodeMapper() = default;

    /**
     * @brief Abstract replacement method for a node.
     *
     * If the given nodes is to be replaced, the handed in node
     * will be destroyed by the mapper and the returned node
     * will become owned by the caller.
     */
    virtual std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const = 0;

    /**
     * @brief Wrapper for any subclass of the RAM node hierarchy performing type casts.
     */
    template <typename T>
    std::unique_ptr<T> operator()(std::unique_ptr<T> node) const {
        std::unique_ptr<RamNode> resPtr =
                (*this)(std::unique_ptr<RamNode>(static_cast<RamNode*>(node.release())));
        assert(nullptr != dynamic_cast<T*>(resPtr.get()) && "Invalid target node!");
        return std::unique_ptr<T>(dynamic_cast<T*>(resPtr.release()));
    }
};

namespace detail {

/**
 * @class LambdaRamNodeMapper
 * @brief A special RamNodeMapper wrapping a lambda conducting node transformations.
 */
template <typename Lambda>
class LambdaRamNodeMapper : public RamNodeMapper {
    const Lambda& lambda;

public:
    /**
     * @brief Constructor for LambdaRamNodeMapper
     */
    LambdaRamNodeMapper(const Lambda& lambda) : lambda(lambda) {}

    /**
     * @brief Applies lambda
     */
    std::unique_ptr<RamNode> operator()(std::unique_ptr<RamNode> node) const override {
        return lambda(std::move(node));
    }
};

}  // namespace detail

/**
 * @brief Creates a node mapper based on a corresponding lambda expression.
 */
template <typename Lambda>
detail::LambdaRamNodeMapper<Lambda> makeLambdaRamMapper(const Lambda& lambda) {
    return detail::LambdaRamNodeMapper<Lambda>(lambda);
}

/**
 *  @class RamNode
 *  @brief RamNode is a superclass for all RAM IR classes.
 */
class RamNode {
public:
    RamNode() = default;

    /*
     * @brief A virtual destructor for RAM nodes
     */
    virtual ~RamNode() = default;

    /**
     * @brief Equivalence check for two RAM nodes
     */
    bool operator==(const RamNode& other) const {
        return this == &other || (typeid(*this) == typeid(other) && equal(other));
    }

    /**
     * @brief Inequality check for two RAM nodes
     */
    bool operator!=(const RamNode& other) const {
        return !(*this == other);
    }

    /**
     * @brief Create a clone (i.e. deep copy) of this node
     */
    virtual RamNode* clone() const = 0;

    /**
     * @brief Apply the mapper to all child nodes
     */
    virtual void apply(const RamNodeMapper& mapper) {}

    /**
     * @brief Rewrite a child node
     */
    virtual void rewrite(const RamNode* oldNode, std::unique_ptr<RamNode> newNode) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> rewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (oldNode == node.get()) {
                return std::move(newNode);
            } else {
                node->apply(makeLambdaRamMapper(rewriter));
                return node;
            }
        };
        apply(makeLambdaRamMapper(rewriter));
    };

    /**
     * @brief Obtain list of all embedded child nodes
     */
    virtual std::vector<const RamNode*> getChildNodes() const {
        return {};
    }

    /**
     * @brief Print RAM node
     */
    virtual void print(std::ostream& out = std::cout) const = 0;

    /**
     * Print RAM on a stream
     */
    friend std::ostream& operator<<(std::ostream& out, const RamNode& node) {
        node.print(out);
        return out;
    }

protected:
    /**
     * @brief Equality check for two RAM nodes.
     * Default action is that nothing needs to be checked.
     */
    virtual bool equal(const RamNode& other) const {
        return true;
    }
};

}  // end of namespace souffle
