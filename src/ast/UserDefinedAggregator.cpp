/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/UserDefinedAggregator.h"
#include "ast/Aggregator.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {
UserDefinedAggregator::UserDefinedAggregator(
        std::string name, Own<Argument> init, Own<Argument> expr, VecOwn<Literal> body, SrcLocation loc)
        : Aggregator(NK_UserDefinedAggregator, std::move(expr), std::move(body), std::move(loc)), name(name),
          initValue(std::move(init)) {}

void UserDefinedAggregator::apply(const NodeMapper& map) {
    Aggregator::apply(map);
    initValue = map(std::move(initValue));
}

Node::NodeVec UserDefinedAggregator::getChildren() const {
    auto res = Aggregator::getChildren();
    res.push_back(initValue.get());
    return res;
}

void UserDefinedAggregator::print(std::ostream& os) const {
    os << "@" << name;
    os << " init: " << *initValue;
    if (targetExpression) {
        os << " " << *targetExpression;
    }
    os << " : { " << join(body) << " }";
}

bool UserDefinedAggregator::equal(const Node& node) const {
    const auto& other = asAssert<UserDefinedAggregator>(node);
    return name == other.name && equal_ptr(targetExpression, other.targetExpression) &&
           equal_ptr(initValue, other.initValue) && equal_targets(body, other.body);
}

UserDefinedAggregator* UserDefinedAggregator::cloning() const {
    return new UserDefinedAggregator(
            name, clone(initValue), clone(targetExpression), clone(body), getSrcLoc());
}

bool UserDefinedAggregator::classof(const Node* n) {
    return n->getKind() == NK_UserDefinedAggregator;
}

}  // namespace souffle::ast
