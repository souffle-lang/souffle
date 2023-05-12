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
Constant::Constant(NodeKind kind, std::string value, SrcLocation loc)
        : Argument(kind, std::move(loc)), constant(std::move(value)) {
    assert(kind >= NK_Constant && kind < NK_LastConstant);
};

void Constant::print(std::ostream& os) const {
    os << getConstant();
}

bool Constant::equal(const Node& node) const {
    const auto& other = asAssert<Constant>(node);
    return constant == other.constant;
}

bool Constant::classof(const Node* n) {
    const NodeKind kind = n->getKind();
    return (kind >= NK_Constant && kind < NK_LastConstant);
}


}  // namespace souffle::ast
