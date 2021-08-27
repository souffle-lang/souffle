/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Negation.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

Negation::Negation(Own<Atom> atom, SrcLocation loc) : Literal(std::move(loc)), atom(std::move(atom)) {
    assert(this->atom != nullptr);
}

void Negation::apply(const NodeMapper& map) {
    atom = map(std::move(atom));
}

Node::NodeVec Negation::getChildren() const {
    return {atom.get()};
}

void Negation::print(std::ostream& os) const {
    os << "!" << *atom;
}

bool Negation::equal(const Node& node) const {
    const auto& other = asAssert<Negation>(node);
    return equal_ptr(atom, other.atom);
}

Negation* Negation::cloning() const {
    return new Negation(clone(atom), getSrcLoc());
}

}  // namespace souffle::ast
