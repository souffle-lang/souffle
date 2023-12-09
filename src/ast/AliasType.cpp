/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/AliasType.h"
#include "souffle/utility/DynamicCasting.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

AliasType::AliasType(QualifiedName name, QualifiedName aliasTypeName, SrcLocation loc)
        : Type(NK_AliasType, std::move(name), std::move(loc)), aliasType(std::move(aliasTypeName)) {}

void AliasType::print(std::ostream& os) const {
    os << ".type " << getQualifiedName() << " = " << getAliasType();
}

bool AliasType::equal(const Node& node) const {
    const auto& other = asAssert<AliasType>(node);
    return getQualifiedName() == other.getQualifiedName() && aliasType == other.aliasType;
}

AliasType* AliasType::cloning() const {
    return new AliasType(getQualifiedName(), getAliasType(), getSrcLoc());
}

bool AliasType::classof(const Node* n) {
    return n->getKind() == NK_AliasType;
}

}  // namespace souffle::ast
