/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/TypeCast.h"

#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/tinyformat.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

TypeCast::TypeCast(Own<Argument> value, QualifiedName type, SrcLocation loc)
        : Argument(std::move(loc)), value(std::move(value)), type(std::move(type)) {
    assert(this->value != nullptr);
}

void TypeCast::setType(QualifiedName type) {
    this->type = std::move(type);
}

Node::NodeVec TypeCast::getChildren() const {
    auto res = Argument::getChildren();
    res.push_back(value.get());
    return res;
}

void TypeCast::apply(const NodeMapper& map) {
    value = map(std::move(value));
}

void TypeCast::print(std::ostream& os) const {
    os << tfm::format("as(%s, %s)", *value, type);
}

bool TypeCast::equal(const Node& node) const {
    const auto& other = asAssert<TypeCast>(node);
    return type == other.type && equal_ptr(value, other.value);
}

TypeCast* TypeCast::cloning() const {
    return new TypeCast(clone(value), type, getSrcLoc());
}
}  // namespace souffle::ast
