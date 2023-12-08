/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/ComponentType.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

ComponentType::ComponentType(std::string name, std::vector<QualifiedName> params, SrcLocation loc)
        : Node(NK_ComponentType, std::move(loc)), name(std::move(name)), typeParams(std::move(params)) {}

void ComponentType::print(std::ostream& os) const {
    os << name;
    if (!typeParams.empty()) {
        os << "<" << join(typeParams) << ">";
    }
}

bool ComponentType::equal(const Node& node) const {
    const auto& other = asAssert<ComponentType>(node);
    return name == other.name && typeParams == other.typeParams;
}

ComponentType* ComponentType::cloning() const {
    return new ComponentType(name, typeParams, getSrcLoc());
}

bool ComponentType::classof(const Node* n) {
    return n->getKind() == NK_ComponentType;
}

}  // namespace souffle::ast
