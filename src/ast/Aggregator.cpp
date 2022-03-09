/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Aggregator.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {
Aggregator::Aggregator(AggregateOp baseOperator, Own<Argument> expr, VecOwn<Literal> body, SrcLocation loc)
        : Argument(std::move(loc)), baseOperator(baseOperator), targetExpression(std::move(expr)),
          body(std::move(body)) {
    // NOTE: targetExpression can be nullptr - it's used e.g. when aggregator
    // has no parameters, such as count: { body }
    assert(allValidPtrs(this->body));
}

std::vector<Literal*> Aggregator::getBodyLiterals() const {
    return toPtrVector(body);
}

void Aggregator::setBodyLiterals(VecOwn<Literal> bodyLiterals) {
    assert(allValidPtrs(body));
    body = std::move(bodyLiterals);
}

void Aggregator::apply(const NodeMapper& map) {
    if (targetExpression) {
        targetExpression = map(std::move(targetExpression));
    }

    mapAll(body, map);
}

void Aggregator::print(std::ostream& os) const {
    os << baseOperator;
    if (targetExpression) {
        os << " " << *targetExpression;
    }
    os << " : { " << join(body) << " }";
}

bool Aggregator::equal(const Node& node) const {
    const auto& other = asAssert<Aggregator>(node);
    return baseOperator == other.baseOperator && equal_ptr(targetExpression, other.targetExpression) &&
           equal_targets(body, other.body);
}

Aggregator* Aggregator::cloning() const {
    return new Aggregator(baseOperator, clone(targetExpression), clone(body), getSrcLoc());
}

Node::NodeVec Aggregator::getChildren() const {
    auto res = Argument::getChildren();
    if (targetExpression) {
        res.push_back(targetExpression.get());
    }
    append(res, makePtrRange(body));
    return res;
}

}  // namespace souffle::ast
