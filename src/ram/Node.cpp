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
    std::function<Own<Node>(Own<Node>)> rewriter = [&](Own<Node> node) -> Own<Node> {
        if (oldNode == node.get()) {
}

Node::ConstChildNodes Node::getChildNodes() const {
    return ConstChildNodes(getChildNodesImpl(), detail::RefCaster());
}

Node::ChildNodes Node::getChildNodes() {
    return ChildNodes(getChildNodesImpl(), detail::ConstCaster());
}

}  // namespace souffle::ram
