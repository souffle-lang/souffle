/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Atom.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

/**
 * @class Atom
 * @brief An atom class
 *
 * An atom representing the use of a relation
 * either in the head or in the body of a clause,
 * e.g., parent(x,y), !parent(x,y), ...
 */
Atom::Atom(QualifiedName name, VecOwn<Argument> args, SrcLocation loc)
        : Literal(std::move(loc)), name(std::move(name)), arguments(std::move(args)) {
    assert(allValidPtrs(arguments));
}

void Atom::setQualifiedName(QualifiedName n) {
    name = std::move(n);
}

void Atom::addArgument(Own<Argument> arg) {
    assert(arg != nullptr);
    arguments.push_back(std::move(arg));
}

std::vector<Argument*> Atom::getArguments() const {
    return toPtrVector(arguments);
}

void Atom::apply(const NodeMapper& map) {
    mapAll(arguments, map);
}

Node::NodeVec Atom::getChildren() const {
    auto cn = makePtrRange(arguments);
    return {cn.begin(), cn.end()};
}

void Atom::print(std::ostream& os) const {
    os << getQualifiedName() << "(" << join(arguments) << ")";
}

bool Atom::equal(const Node& node) const {
    const auto& other = asAssert<Atom>(node);
    return name == other.name && equal_targets(arguments, other.arguments);
}

Atom* Atom::cloning() const {
    return new Atom(name, clone(arguments), getSrcLoc());
}

}  // namespace souffle::ast
