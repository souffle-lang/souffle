/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Program.h"
#include "ast/utility/NodeMapper.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <utility>

namespace souffle::ast {

std::vector<Type*> Program::getTypes() const {
    return toPtrVector(types);
}

std::vector<Relation*> Program::getRelations() const {
    return toPtrVector(relations);
}

std::vector<Clause*> Program::getClauses() const {
    return toPtrVector(clauses);
}

std::vector<FunctorDeclaration*> Program::getFunctorDeclarations() const {
    return toPtrVector(functors);
}

std::vector<Directive*> Program::getDirectives() const {
    return toPtrVector(directives);
}

void Program::addDirective(Own<Directive> directive) {
    assert(directive && "NULL directive");
    directives.push_back(std::move(directive));
}

void Program::addRelation(Own<Relation> relation) {
    assert(relation != nullptr);
    [[maybe_unused]] auto* existingRelation = getIf(getRelations(), [&](const Relation* current) {
        return current->getQualifiedName() == relation->getQualifiedName();
    });
    assert(existingRelation == nullptr && "Redefinition of relation!");
    relations.push_back(std::move(relation));
}

bool Program::removeRelationDecl(const QualifiedName& name) {
    // FIXME: Refactor to std::remove/erase
    for (auto it = relations.begin(); it != relations.end(); it++) {
        const auto& rel = *it;
        if (rel->getQualifiedName() == name) {
            relations.erase(it);
            return true;
        }
    }
    return false;
}

void Program::setClauses(VecOwn<Clause> newClauses) {
    assert(allValidPtrs(newClauses));
    clauses = std::move(newClauses);
}

void Program::addClause(Own<Clause> clause) {
    assert(clause != nullptr && "Undefined clause");
    assert(clause->getHead() != nullptr && "Undefined head of the clause");
    clauses.push_back(std::move(clause));
}

bool Program::removeClause(const Clause* clause) {
    // FIXME: Refactor to std::remove/erase
    for (auto it = clauses.begin(); it != clauses.end(); it++) {
        if (**it == *clause) {
            clauses.erase(it);
            return true;
        }
    }
    return false;
}

bool Program::removeDirective(const Directive* directive) {
    // FIXME: Refactor to std::remove/erase
    for (auto it = directives.begin(); it != directives.end(); it++) {
        if (**it == *directive) {
            directives.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<Component*> Program::getComponents() const {
    return toPtrVector(components);
}

void Program::addType(Own<Type> type) {
    assert(type != nullptr);
    [[maybe_unused]] auto* existingType = getIf(getTypes(),
            [&](const Type* current) { return current->getQualifiedName() == type->getQualifiedName(); });
    assert(existingType == nullptr && "Redefinition of type!");
    types.push_back(std::move(type));
}

void Program::addPragma(Own<Pragma> pragma) {
    assert(pragma && "NULL pragma");
    pragmas.push_back(std::move(pragma));
}

void Program::addFunctorDeclaration(Own<FunctorDeclaration> f) {
    assert(f != nullptr);
    [[maybe_unused]] auto* existingFunctorDecl = getIf(getFunctorDeclarations(),
            [&](const FunctorDeclaration* current) { return current->getName() == f->getName(); });
    assert(existingFunctorDecl == nullptr && "Redefinition of functor!");
    functors.push_back(std::move(f));
}

std::vector<ComponentInit*> Program::getComponentInstantiations() const {
    return toPtrVector(instantiations);
}

void Program::clearComponents() {
    components.clear();
    instantiations.clear();
}

void Program::apply(const NodeMapper& map) {
    mapAll(pragmas, map);
    mapAll(components, map);
    mapAll(instantiations, map);
    mapAll(functors, map);
    mapAll(types, map);
    mapAll(relations, map);
    mapAll(clauses, map);
    mapAll(directives, map);
}

Node::NodeVec Program::getChildNodesImpl() const {
    std::vector<const Node*> res;
    append(res, makePtrRange(pragmas));
    append(res, makePtrRange(components));
    append(res, makePtrRange(instantiations));
    append(res, makePtrRange(functors));
    append(res, makePtrRange(types));
    append(res, makePtrRange(relations));
    append(res, makePtrRange(clauses));
    append(res, makePtrRange(directives));
    return res;
}

void Program::print(std::ostream& os) const {
    auto show = [&](auto&& xs, char const* sep = "\n") {
        if (!xs.empty()) os << join(xs, sep) << "\n";
    };

    show(pragmas, "\n\n");
    show(components);
    show(instantiations);
    show(types);
    show(functors);
    show(relations);
    show(clauses, "\n\n");
    show(directives, "\n\n");
}

bool Program::equal(const Node& node) const {
    const auto& other = asAssert<Program>(node);
    // clang-format off
    return equal_targets(pragmas, other.pragmas) &&
           equal_targets(components, other.components) &&
           equal_targets(instantiations, other.instantiations) &&
           equal_targets(functors, other.functors) &&
           equal_targets(types, other.types) &&
           equal_targets(relations, other.relations) &&
           equal_targets(clauses, other.clauses) &&
           equal_targets(directives, other.directives);
    // clang-format on
}

void Program::addComponent(Own<Component> component) {
    assert(component && "NULL component");
    components.push_back(std::move(component));
}

void Program::addInstantiation(Own<ComponentInit> instantiation) {
    assert(instantiation && "NULL instantiation");
    instantiations.push_back(std::move(instantiation));
}

Program* Program::cloning() const {
    auto res = new Program();
    res->pragmas = clone(pragmas);
    res->components = clone(components);
    res->instantiations = clone(instantiations);
    res->types = clone(types);
    res->functors = clone(functors);
    res->relations = clone(relations);
    res->clauses = clone(clauses);
    res->directives = clone(directives);
    return res;
}

}  // namespace souffle::ast
