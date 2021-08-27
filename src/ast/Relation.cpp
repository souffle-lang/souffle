/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Relation.h"

#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

Relation::Relation(QualifiedName name, SrcLocation loc) : Node(std::move(loc)), name(std::move(name)) {}

void Relation::setQualifiedName(QualifiedName n) {
    name = std::move(n);
}

void Relation::addAttribute(Own<Attribute> attr) {
    assert(attr && "Undefined attribute");
    attributes.push_back(std::move(attr));
}

void Relation::setAttributes(VecOwn<Attribute> attrs) {
    assert(allValidPtrs(attrs));
    attributes = std::move(attrs);
}

std::vector<Attribute*> Relation::getAttributes() const {
    return toPtrVector(attributes);
}

void Relation::addDependency(Own<FunctionalConstraint> fd) {
    assert(fd != nullptr);
    functionalDependencies.push_back(std::move(fd));
}

std::vector<FunctionalConstraint*> Relation::getFunctionalDependencies() const {
    return toPtrVector(functionalDependencies);
}

void Relation::apply(const NodeMapper& map) {
    mapAll(attributes, map);
}

Node::NodeVec Relation::getChildren() const {
    auto rn = makePtrRange(attributes);
    return {rn.begin(), rn.end()};
}

void Relation::print(std::ostream& os) const {
    os << ".decl " << getQualifiedName() << "(" << join(attributes, ", ") << ")" << join(qualifiers, " ")
       << " " << representation;
    if (!functionalDependencies.empty()) {
        os << " choice-domain " << join(functionalDependencies, ", ");
    }
}

bool Relation::equal(const Node& node) const {
    const auto& other = asAssert<Relation>(node);
    return name == other.name && equal_targets(attributes, other.attributes) &&
           qualifiers == other.qualifiers &&
           equal_targets(functionalDependencies, other.functionalDependencies) &&
           representation == other.representation;
}

Relation* Relation::cloning() const {
    auto res = new Relation(name, getSrcLoc());
    res->attributes = clone(attributes);
    res->qualifiers = qualifiers;
    res->functionalDependencies = clone(functionalDependencies);
    res->representation = representation;
    return res;
}

}  // namespace souffle::ast
