/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/ComponentInit.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

ComponentInit::ComponentInit(std::string name, Own<ComponentType> type, SrcLocation loc)
        : Node(std::move(loc)), instanceName(std::move(name)), componentType(std::move(type)) {
    assert(componentType);
}

void ComponentInit::setInstanceName(std::string name) {
    instanceName = std::move(name);
}

void ComponentInit::setComponentType(Own<ComponentType> type) {
    assert(type != nullptr);
    componentType = std::move(type);
}

void ComponentInit::apply(const NodeMapper& mapper) {
    componentType = mapper(std::move(componentType));
}

Node::NodeVec ComponentInit::getChildren() const {
    return {componentType.get()};
}

void ComponentInit::print(std::ostream& os) const {
    os << ".init " << instanceName << " = " << *componentType;
}

bool ComponentInit::equal(const Node& node) const {
    const auto& other = asAssert<ComponentInit>(node);
    return instanceName == other.instanceName && *componentType == *other.componentType;
}

ComponentInit* ComponentInit::cloning() const {
    return new ComponentInit(instanceName, clone(componentType), getSrcLoc());
}
}  // namespace souffle::ast
