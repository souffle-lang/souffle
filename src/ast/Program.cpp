/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Program.h"

#include "ast/Node.h"
#include "ast/utility/Utils.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <utility>

namespace souffle {

using RelationInfo = ast::Program::RelationInfo;
using RelationInfoMap = ast::Program::RelationInfoMap;

template <typename SeqOwnT>
auto toPtrVector(RelationInfoMap const& map, SeqOwnT RelationInfo::*member, ast::QualifiedName const& name) {
    using A = typename SeqOwnT::value_type::element_type;
    std::vector<A*> ys;

    auto it = map.find(name);
    if (it != map.end()) {
        for (auto&& x : it->second.*member) {
            assert(name == getName(*x));
            ys.push_back(x.get());
        }
    }

    return ys;
}

template <typename SeqOwnT>
auto toPtrVector(RelationInfoMap const& map, SeqOwnT RelationInfo::*member) {
    using A = typename SeqOwnT::value_type::element_type;
    std::vector<A*> ys;

    for ([[maybe_unused]] auto&& [k, info] : map) {
        for (auto&& x : info.*member) {
            assert(k == getName(*x));
            ys.push_back(x.get());
        }
    }

    return ys;
}

namespace {
template <typename A, typename SeqOwnT>
auto erase(RelationInfoMap& map, SeqOwnT RelationInfo::*member, A const& elem) {
    auto it = map.find(getName(elem));
    if (it == map.end()) {
        assert(false &&
                "attempted to remove something not owned by the program. this is symptomatic of a bug");
        return false;
    }

    auto&& [_, info] = *it;
    auto& xs = info.*member;
    auto xs_it = std::remove_if(xs.begin(), xs.end(), [&](auto&& x) { return x.get() == &elem; });
    if (xs_it == xs.end()) {
        assert(false &&
                "attempted to remove something not owned by the program. this is symptomatic of a bug");
        return false;
    }

    xs.erase(xs_it);

    if (info.decls.empty() && info.clauses.empty() && info.directives.empty()) {
        // everything was removed. can drop the bucket.
        map.erase(it);
    }

    return true;
}
}  // namespace

template <typename SeqOwnT>
auto mapAll(RelationInfoMap& map, SeqOwnT RelationInfo::*member, const ast::NodeMapper& mapper) {
    for ([[maybe_unused]] auto&& [k, info] : map) {
        // assume `info` is well formed (no wrong names). only bother checking post-condition.
        mapAll(info.*member, mapper);

#ifndef NDEBUG
        for (auto&& x : info.*member)
            assert(k == getName(*x));
#endif
    }
}

template <typename SeqOwnT>
void append(std::vector<ast::Node const*>& res, RelationInfoMap const& map, SeqOwnT RelationInfo::*member) {
    for ([[maybe_unused]] auto&& [k, info] : map) {
        for (auto&& x : info.*member) {
            assert(k == getName(*x));
            res.push_back(x.get());
        }
    }
}

}  // namespace souffle

namespace souffle::ast {

using souffle::clone;

Program::Program() : Node(NK_Program) {}

Program::RelationInfo clone(Program::RelationInfo const& x) {
    return {clone(x.decls), clone(x.clauses), clone(x.directives)};
}

std::vector<Type*> Program::getTypes() const {
    return toPtrVector(types);
}

std::vector<Lattice*> Program::getLattices() const {
    return toPtrVector(lattices);
}

std::vector<Relation*> Program::getRelations() const {
    return toPtrVector(relations, &RelationInfo::decls);
}

Relation* Program::getRelation(QualifiedName const& name) const {
    auto it = relations.find(name);
    if (it == relations.end()) return nullptr;

    auto&& [_, info] = *it;
    if (info.decls.empty()) return nullptr;

    return info.decls.front().get();
}

Relation* Program::getRelation(Atom const& x) const {
    return getRelation(getName(x));
}

Relation* Program::getRelation(Clause const& x) const {
    return getRelation(getName(x));
}

Relation* Program::getRelation(Directive const& x) const {
    return getRelation(getName(x));
}

std::vector<Relation*> Program::getRelationAll(QualifiedName const& name) const {
    return toPtrVector(relations, &RelationInfo::decls, name);
}

std::vector<Clause*> Program::getClauses() const {
    return toPtrVector(relations, &RelationInfo::clauses);
}

std::vector<Clause*> Program::getClauses(QualifiedName const& name) const {
    return toPtrVector(relations, &RelationInfo::clauses, name);
}

std::vector<FunctorDeclaration*> Program::getFunctorDeclarations() const {
    return toPtrVector(functors);
}

std::vector<Directive*> Program::getDirectives() const {
    return toPtrVector(relations, &RelationInfo::directives);
}

std::vector<Directive*> Program::getDirectives(QualifiedName const& name) const {
    return toPtrVector(relations, &RelationInfo::directives, name);
}

void Program::addDirective(Own<Directive> directive) {
    assert(directive && "NULL directive");
    auto& info = relations[directive->getQualifiedName()];
    info.directives.push_back(std::move(directive));
}

void Program::addRelation(Own<Relation> relation) {
    assert(relation != nullptr);
    auto& info = relations[relation->getQualifiedName()];
    assert(info.decls.empty() && "Redefinition of relation!");
    info.decls.push_back(std::move(relation));
}

bool Program::removeRelation(QualifiedName const& name) {
    return 0 < relations.erase(name);
}

void Program::removeRelation(Relation const& r) {
    auto name = r.getQualifiedName();
    erase(relations, &RelationInfo::decls, r);  // run just for assert/sancheck
    removeRelation(name);
}

void Program::addClause(Own<Clause> clause) {
    assert(clause != nullptr && "Undefined clause");
    assert(clause_visit_in_progress == 0 && "Don't modify program clause collection mid-traversal");
    auto& info = relations[getName(*clause)];
    info.clauses.push_back(std::move(clause));
}

void Program::addClauses(VecOwn<Clause> clauses) {
    for (Own<Clause>& cl : clauses)
        addClause(std::move(cl));
}

void Program::removeClause(const Clause& clause) {
    assert(clause_visit_in_progress == 0 && "Don't modify program clause collection mid-traversal");
    erase(relations, &RelationInfo::clauses, clause);
}

void Program::removeClauses(span<Clause const* const> clauses) {
    for (auto&& cl : clauses) {
        assert(cl);
        removeClause(*cl);
    }
}

void Program::removeDirective(const Directive& directive) {
    erase(relations, &RelationInfo::directives, directive);
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

void Program::addLattice(Own<Lattice> lattice) {
    assert(lattice != nullptr);
    [[maybe_unused]] auto* existingLattice = getIf(getLattices(), [&](const Lattice* current) {
        return current->getQualifiedName() == lattice->getQualifiedName();
    });
    assert(existingLattice == nullptr && "Redefinition of lattice!");
    lattices.push_back(std::move(lattice));
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

std::vector<ComponentInit*> Program::getInstantiations() const {
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
    mapAll(lattices, map);
    mapAll(relations, &RelationInfo::decls, map);
    mapAll(relations, &RelationInfo::clauses, map);
    mapAll(relations, &RelationInfo::directives, map);
}

Node::NodeVec Program::getChildren() const {
    std::vector<const Node*> res;
    append(res, makePtrRange(pragmas));
    append(res, makePtrRange(components));
    append(res, makePtrRange(instantiations));
    append(res, makePtrRange(functors));
    append(res, makePtrRange(types));
    append(res, makePtrRange(lattices));
    append(res, relations, &RelationInfo::decls);
    append(res, relations, &RelationInfo::clauses);
    append(res, relations, &RelationInfo::directives);
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
    show(lattices);
    show(functors);
    show(getRelations());
    show(getClauses(), "\n\n");
    show(getDirectives(), "\n\n");
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
    res->lattices = clone(lattices);
    res->functors = clone(functors);
    res->relations = clone(relations);
    return res;
}

bool Program::classof(const Node* n) {
    return n->getKind() == NK_Program;
}

bool Program::equal(const Node&) const {
    assert("never needed" && false);
    throw("not implemented");
}

}  // namespace souffle::ast
