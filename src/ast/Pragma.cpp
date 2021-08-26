/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Pragma.h"
#include "souffle/utility/DynamicCasting.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

Pragma::Pragma(std::string key, std::string value, SrcLocation loc)
        : Node(std::move(loc)), key(std::move(key)), value(std::move(value)) {}

void Pragma::print(std::ostream& os) const {
    os << ".pragma " << key << " " << value << "\n";
}

bool Pragma::equal(const Node& node) const {
    const auto& other = asAssert<Pragma>(node);
    return other.key == key && other.value == value;
}

Pragma* Pragma::cloning() const {
    return new Pragma(key, value, getSrcLoc());
}

}  // namespace souffle::ast
