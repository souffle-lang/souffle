/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Component.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {
void Component::setComponentType(Own<ComponentType> other) {
    assert(other != nullptr);
    componentType = std::move(other);
}

const std::vector<ComponentType*> Component::getBaseComponents() const {
    return toPtrVector(baseComponents);
}

void Component::addBaseComponent(Own<ComponentType> component) {
    assert(component != nullptr);
    baseComponents.push_back(std::move(component));
}

void Component::addType(Own<Type> t) {
    assert(t != nullptr);
    types.push_back(std::move(t));
}

std::vector<Type*> Component::getTypes() const {
    return toPtrVector(types);
}

void Component::copyBaseComponents(const Component& other) {
    baseComponents = clone(other.baseComponents);
}

void Component::addRelation(Own<Relation> r) {
    assert(r != nullptr);
    relations.push_back(std::move(r));
}

std::vector<Relation*> Component::getRelations() const {
    return toPtrVector(relations);
}

void Component::addClause(Own<Clause> c) {
    assert(c != nullptr);
    clauses.push_back(std::move(c));
}

std::vector<Clause*> Component::getClauses() const {
    return toPtrVector(clauses);
}

void Component::addDirective(Own<Directive> directive) {
    assert(directive != nullptr);
    directives.push_back(std::move(directive));
}

std::vector<Directive*> Component::getDirectives() const {
    return toPtrVector(directives);
}

void Component::addComponent(Own<Component> c) {
    assert(c != nullptr);
    components.push_back(std::move(c));
}

std::vector<Component*> Component::getComponents() const {
    return toPtrVector(components);
}

void Component::addInstantiation(Own<ComponentInit> i) {
    assert(i != nullptr);
    instantiations.push_back(std::move(i));
}

std::vector<ComponentInit*> Component::getInstantiations() const {
    return toPtrVector(instantiations);
}

void Component::apply(const NodeMapper& mapper) {
    componentType = mapper(std::move(componentType));
    mapAll(baseComponents, mapper);
    mapAll(components, mapper);
    mapAll(instantiations, mapper);
    mapAll(types, mapper);
    mapAll(relations, mapper);
    mapAll(clauses, mapper);
    mapAll(directives, mapper);
}

Node::NodeVec Component::getChildren() const {
    std::vector<const Node*> res;
    res.push_back(componentType.get());
    append(res, makePtrRange(baseComponents));
    append(res, makePtrRange(components));
    append(res, makePtrRange(instantiations));
    append(res, makePtrRange(types));
    append(res, makePtrRange(relations));
    append(res, makePtrRange(clauses));
    append(res, makePtrRange(directives));
    return res;
}

void Component::print(std::ostream& os) const {
    auto show = [&](auto&& xs, char const* sep = "\n", char const* prefix = "") {
        if (xs.empty()) return;
        os << prefix << join(xs, sep) << "\n";
    };

    os << ".comp " << *componentType << " ";
    show(baseComponents, ",", ": ");
    os << "{\n";
    show(components);
    show(instantiations);
    show(types);
    show(relations);
    show(overrideRules, ",", ".");
    show(clauses, "\n\n");
    show(directives, "\n\n");
    os << "}\n";
}

bool Component::equal(const Node& node) const {
    const auto& other = asAssert<Component>(node);

    // FIXME (?): This pointer comparison is either irrelevant or should
    // be moved to operator== in Node. (Since e.g. Program doesn't check pointer
    // equlity)
    return (&componentType == &other.componentType) ||
           // clang-format off
           (equal_targets(baseComponents, other.baseComponents) &&
            equal_targets(components, other.components) &&
            equal_targets(instantiations, other.instantiations) &&
            equal_targets(types, other.types) &&
            equal_targets(relations, other.relations) &&
            equal_targets(clauses, other.clauses) &&
            equal_targets(directives, other.directives) &&
            overrideRules != other.overrideRules);
           // clang-format off
}

Component* Component::cloning() const {
    auto* res = new Component();
    res->componentType = clone(componentType);
    res->baseComponents = clone(baseComponents);
    res->components = clone(components);
    res->instantiations = clone(instantiations);
    res->types = clone(types);
    res->relations = clone(relations);
    res->clauses = clone(clauses);
    res->directives = clone(directives);
    res->overrideRules = overrideRules;
    return res;
}

}  // namespace souffle::ast
