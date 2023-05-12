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
Aggregator::Aggregator(NodeKind kind, Own<Argument> expr, VecOwn<Literal> body, SrcLocation loc)
        : Argument(kind, std::move(loc)), targetExpression(std::move(expr)), body(std::move(body)) {
    // NOTE: targetExpression can be nullptr - it's used e.g. when aggregator
    // has no parameters, such as count: { body }
    assert(allValidPtrs(this->body));
    assert(kind >= NK_Aggregator && kind < NK_LastAggregator);
}

std::vector<Literal*> Aggregator::getBodyLiterals() const {
    return toPtrVector(body);
}

VecOwn<Literal> Aggregator::setBodyLiterals(VecOwn<Literal> bodyLiterals) {
    assert(allValidPtrs(body));
    auto oldBody = std::move(body);
    body = std::move(bodyLiterals);
    return oldBody;
}

void Aggregator::apply(const NodeMapper& map) {
    if (targetExpression) {
        targetExpression = map(std::move(targetExpression));
    }

    mapAll(body, map);
}

bool Aggregator::classof(const Node* n) {
    const NodeKind kind = n->getKind();
    return (kind >= NK_Aggregator && kind < NK_LastAggregator);
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
