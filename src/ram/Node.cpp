/*
 * Souffle - A Datalog Compiler
 * Copyright Copyright (c) 2021,, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Node.cpp
 *
 * Implementation of RAM node
 *
 ***********************************************************************/

#include "ram/Node.h"
#include "souffle/utility/NodeMapper.h"
#include <cassert>
#include <functional>
#include <memory>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle::ram {

void Node::rewrite(const Node* oldNode, Own<Node> newNode) {
    assert(oldNode != nullptr && "old node is a null-pointer");
    assert(newNode != nullptr && "new node is a null-pointer");
    mapChildFrontier(*this, [=, newNode = std::move(newNode)](Own<Node> node) mutable {
        // clear `oldNode` to mark that we're done, and to avoid touching an invalidated ptr (UB)
        if (oldNode == node.get()) {
            oldNode = nullptr;
            node = std::move(newNode);
        }

        return std::pair{std::move(node), !oldNode};
    });
}

Node::ConstChildNodes Node::getChildNodes() const {
    return ConstChildNodes(getChildNodesImpl(), detail::RefCaster());
}

Node::ChildNodes Node::getChildNodes() {
    return ChildNodes(getChildNodesImpl(), detail::ConstCaster());
}

}  // namespace souffle::ram
