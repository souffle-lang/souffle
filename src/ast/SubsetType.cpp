/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/SubsetType.h"
#include "souffle/utility/DynamicCasting.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

SubsetType::SubsetType(QualifiedName name, QualifiedName baseTypeName, SrcLocation loc)
        : Type(std::move(name), std::move(loc)), baseType(std::move(baseTypeName)) {}

void SubsetType::print(std::ostream& os) const {
    os << ".type " << getQualifiedName() << " <: " << getBaseType();
}

bool SubsetType::equal(const Node& node) const {
    const auto& other = asAssert<SubsetType>(node);
    return getQualifiedName() == other.getQualifiedName() && baseType == other.baseType;
}

SubsetType* SubsetType::cloning() const {
    return new SubsetType(getQualifiedName(), getBaseType(), getSrcLoc());
}

}  // namespace souffle::ast
