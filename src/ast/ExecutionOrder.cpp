/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/ExecutionOrder.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

ExecutionOrder::ExecutionOrder(ExecOrder order, SrcLocation loc)
        : Node(NK_ExecutionOrder, std::move(loc)), order(std::move(order)) {}

void ExecutionOrder::print(std::ostream& out) const {
    out << "(" << join(order) << ")";
}

bool ExecutionOrder::equal(const Node& node) const {
    const auto& other = asAssert<ExecutionOrder>(node);
    return order == other.order;
}

ExecutionOrder* ExecutionOrder::cloning() const {
    return new ExecutionOrder(order, getSrcLoc());
}

bool ExecutionOrder::classof(const Node* n) {
    return n->getKind() == NK_ExecutionOrder;
}

}  // namespace souffle::ast
