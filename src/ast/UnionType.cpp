/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/UnionType.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <utility>

namespace souffle::ast {
UnionType::UnionType(QualifiedName name, std::vector<QualifiedName> types, SrcLocation loc)
        : Type(NK_UnionType, std::move(name), std::move(loc)), types(std::move(types)) {}

void UnionType::add(QualifiedName type) {
    types.push_back(std::move(type));
}

void UnionType::setType(std::size_t idx, QualifiedName type) {
    types.at(idx) = std::move(type);
}

void UnionType::print(std::ostream& os) const {
    os << ".type " << getQualifiedName() << " = " << join(types, " | ");
}

bool UnionType::equal(const Node& node) const {
    const auto& other = asAssert<UnionType>(node);
    return getQualifiedName() == other.getQualifiedName() && types == other.types;
}

UnionType* UnionType::cloning() const {
    return new UnionType(getQualifiedName(), types, getSrcLoc());
}

bool UnionType::classof(const Node* n) {
    return n->getKind() == NK_UnionType;
}

}  // namespace souffle::ast
