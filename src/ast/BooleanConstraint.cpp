/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/BooleanConstraint.h"
#include "souffle/utility/DynamicCasting.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

/**
 * @class BooleanConstraint
 * @brief Boolean constraint class
 *
 * Example:
 *       true
 *
 * Boolean constraint representing either the 'true' or the 'false' value
 */
BooleanConstraint::BooleanConstraint(bool truthValue, SrcLocation loc)
        : Constraint(std::move(loc)), truthValue(truthValue) {}

void BooleanConstraint::print(std::ostream& os) const {
    os << (truthValue ? "true" : "false");
}

bool BooleanConstraint::equal(const Node& node) const {
    const auto& other = asAssert<BooleanConstraint>(node);
    return truthValue == other.truthValue;
}

BooleanConstraint* BooleanConstraint::cloning() const {
    return new BooleanConstraint(truthValue, getSrcLoc());
}

}  // namespace souffle::ast
