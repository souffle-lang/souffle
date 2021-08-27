/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/BinaryConstraint.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

BinaryConstraint::BinaryConstraint(BinaryConstraintOp o, Own<Argument> ls, Own<Argument> rs, SrcLocation loc)
        : Constraint(std::move(loc)), operation(o), lhs(std::move(ls)), rhs(std::move(rs)) {
    assert(lhs != nullptr);
    assert(rhs != nullptr);
}

void BinaryConstraint::apply(const NodeMapper& map) {
    lhs = map(std::move(lhs));
    rhs = map(std::move(rhs));
}

Node::NodeVec BinaryConstraint::getChildren() const {
    return {lhs.get(), rhs.get()};
}

void BinaryConstraint::print(std::ostream& os) const {
    if (isInfixFunctorOp(operation)) {
        os << *lhs << " " << operation << " " << *rhs;
    } else {
        os << operation << "(" << *lhs << ", " << *rhs << ")";
    }
}

bool BinaryConstraint::equal(const Node& node) const {
    const auto& other = asAssert<BinaryConstraint>(node);
    return operation == other.operation && equal_ptr(lhs, other.lhs) && equal_ptr(rhs, other.rhs);
}

BinaryConstraint* BinaryConstraint::cloning() const {
    return new BinaryConstraint(operation, clone(lhs), clone(rhs), getSrcLoc());
}

}  // namespace souffle::ast
