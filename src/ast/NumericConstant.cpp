/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/NumericConstant.h"
#include "souffle/utility/DynamicCasting.h"
#include <cassert>
#include <utility>

namespace souffle::ast {

NumericConstant::NumericConstant(RamSigned value) : Constant(std::to_string(value)), fixedType(Type::Int) {}

NumericConstant::NumericConstant(std::string constant, SrcLocation loc)
        : Constant(std::move(constant), std::move(loc)) {}

NumericConstant::NumericConstant(std::string constant, std::optional<Type> fixedType, SrcLocation loc)
        : Constant(std::move(constant), std::move(loc)), fixedType(fixedType) {}

bool NumericConstant::equal(const Node& node) const {
    const auto& other = asAssert<NumericConstant>(node);
    return Constant::equal(node) && fixedType == other.fixedType;
}

NumericConstant* NumericConstant::cloning() const {
    return new NumericConstant(getConstant(), getFixedType(), getSrcLoc());
}

}  // namespace souffle::ast
