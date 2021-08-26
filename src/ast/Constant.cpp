/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Constant.h"
#include "souffle/utility/DynamicCasting.h"
#include <ostream>
#include <utility>

namespace souffle::ast {
Constant::Constant(std::string value, SrcLocation loc)
        : Argument(std::move(loc)), constant(std::move(value)){};

void Constant::print(std::ostream& os) const {
    os << getConstant();
}

bool Constant::equal(const Node& node) const {
    const auto& other = asAssert<Constant>(node);
    return constant == other.constant;
}

}  // namespace souffle::ast
