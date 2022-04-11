/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/ExecutionPlan.h"

#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

/** Set execution order for a given rule version */
void ExecutionPlan::setOrderFor(std::size_t version, Own<ExecutionOrder> plan) {
    assert(plan != nullptr);
    plans[version] = std::move(plan);
}

/** Get orders */
std::map<std::size_t, const ExecutionOrder*> ExecutionPlan::getOrders() const {
    std::map<std::size_t, const ExecutionOrder*> result;
    for (auto& plan : plans) {
        result.insert(std::make_pair(plan.first, plan.second.get()));
    }
    return result;
}

void ExecutionPlan::apply(const NodeMapper& map) {
    for (auto& plan : plans) {
        plan.second = map(std::move(plan.second));
    }
}

Node::NodeVec ExecutionPlan::getChildren() const {
    auto ran = makeTransformRange(plans, [](auto const& pair) { return pair.second.get(); });
    return {ran.begin(), ran.end()};
}

void ExecutionPlan::print(std::ostream& out) const {
    if (!plans.empty()) {
        out << " .plan ";
        out << join(plans, ", ",
                [](std::ostream& os, const auto& arg) { os << arg.first << ":" << *arg.second; });
    }
}

bool ExecutionPlan::equal(const Node& node) const {
    const auto& other = asAssert<ExecutionPlan>(node);
    return equal_targets(plans, other.plans);
}

ExecutionPlan* ExecutionPlan::cloning() const {
    auto res = mk<ExecutionPlan>(getSrcLoc());
    for (auto& plan : plans) {
        res->setOrderFor(plan.first, clone(plan.second));
    }
    return res.release();
}
}  // namespace souffle::ast
