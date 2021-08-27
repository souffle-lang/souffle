/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */
#include "ast/Node.h"
#include <typeinfo>
#include <utility>

namespace souffle::ast {
Node::Node(SrcLocation loc) : location(std::move(loc)) {}

/** Set source location for the Node */
void Node::setSrcLoc(SrcLocation l) {
    location = std::move(l);
}

bool Node::operator==(const Node& other) const {
    if (this == &other) {
        return true;
    }

    return typeid(*this) == typeid(*&other) && equal(other);
}

Own<Node> Node::cloneImpl() const {
    return Own<Node>(cloning());
}

/** Apply the mapper to all child nodes */
void Node::apply(const NodeMapper& /* mapper */) {}

Node::ConstChildNodes Node::getChildNodes() const {
    return ConstChildNodes(getChildren(), detail::RefCaster());
}

Node::ChildNodes Node::getChildNodes() {
    return ChildNodes(getChildren(), detail::ConstCaster());
}

std::ostream& operator<<(std::ostream& out, const Node& node) {
    node.print(out);
    return out;
}

bool Node::equal(const Node&) const {
    // FIXME: Change to this == &other?
    return true;
}

Node::NodeVec Node::getChildren() const {
    return {};
}

}  // namespace souffle::ast
