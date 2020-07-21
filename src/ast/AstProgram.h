/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstProgram.h
 *
 * Define a class that represents a Datalog program consisting of types,
 * relations, and clauses.
 *
 ***********************************************************************/

#pragma once

#include "ast/AstClause.h"
#include "ast/AstComponent.h"
#include "ast/AstFunctorDeclaration.h"
#include "ast/AstIO.h"
#include "ast/AstNode.h"
#include "ast/AstPragma.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstType.h"
#include "ast/AstUtils.h"
#include "utility/ContainerUtil.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle {

/**
 *  Intermediate representation of a datalog program
 *          that consists of relations, clauses and types
 */
class AstProgram : public AstNode {
public:
    /** get types */
    std::vector<AstType*> getTypes() const {
        return toPtrVector(types);
    }

    /** get relations */
    std::vector<AstRelation*> getRelations() const {
        return toPtrVector(relations);
    }

    /** get clauses */
    std::vector<AstClause*> getClauses() const {
        return toPtrVector(clauses);
    }

    /** get functor declarations */
    std::vector<AstFunctorDeclaration*> getFunctorDeclarations() const {
        return toPtrVector(functors);
    }

    /** get io directives */
    std::vector<AstIO*> getIOs() const {
        return toPtrVector(ios);
    }

    /** get pragma directives */
    const VecOwn<AstPragma>& getPragmaDirectives() const {
        return pragmaDirectives;
    }

    /* add relation */
    void addRelation(Own<AstRelation> r) {
        assert(getRelation(*this, r->getQualifiedName()) == nullptr && "Redefinition of relation!");
        relations.push_back(std::move(r));
    }

    /** remove relation */
    bool removeRelation(const AstQualifiedName& name) {
        for (auto it = relations.begin(); it != relations.end(); it++) {
            const auto& rel = *it;
            if (rel->getQualifiedName() == name) {
                removeRelationClauses(*this, name);
                removeRelationIOs(*this, name);
                relations.erase(it);
                return true;
            }
        }
        return false;
    }

    void setClauses(VecOwn<AstClause> newClauses) {
        clauses = std::move(newClauses);
    }

    /** add a clause */
    void addClause(Own<AstClause> clause) {
        assert(clause != nullptr && "Undefined clause");
        assert(clause->getHead() != nullptr && "Undefined head of the clause");
        clauses.push_back(std::move(clause));
    }

    /** remove a clause */
    bool removeClause(const AstClause* clause) {
        for (auto it = clauses.begin(); it != clauses.end(); it++) {
            if (**it == *clause) {
                clauses.erase(it);
                return true;
            }
        }
        return false;
    }

    /** remove an io */
    bool removeIO(const AstIO* io) {
        for (auto it = ios.begin(); it != ios.end(); it++) {
            if (**it == *io) {
                ios.erase(it);
                return true;
            }
        }
        return false;
    }

    /** get components */
    std::vector<AstComponent*> getComponents() const {
        return toPtrVector(components);
    }

    /** get component instantiation */
    std::vector<AstComponentInit*> getComponentInstantiations() const {
        return toPtrVector(instantiations);
    }

    AstProgram* clone() const override {
        auto res = new AstProgram();
        res->pragmaDirectives = souffle::clone(pragmaDirectives);
        res->components = souffle::clone(components);
        res->instantiations = souffle::clone(instantiations);
        res->types = souffle::clone(types);
        res->functors = souffle::clone(functors);
        res->relations = souffle::clone(relations);
        res->clauses = souffle::clone(clauses);
        res->ios = souffle::clone(ios);
        return res;
    }

    void apply(const AstNodeMapper& map) override {
        for (auto& cur : pragmaDirectives) {
            cur = map(std::move(cur));
        }
        for (auto& cur : components) {
            cur = map(std::move(cur));
        }
        for (auto& cur : instantiations) {
            cur = map(std::move(cur));
        }
        for (auto& cur : functors) {
            cur = map(std::move(cur));
        }
        for (auto& cur : types) {
            cur = map(std::move(cur));
        }
        for (auto& cur : relations) {
            cur = map(std::move(cur));
        }
        for (auto& cur : clauses) {
            cur = map(std::move(cur));
        }
        for (auto& cur : ios) {
            cur = map(std::move(cur));
        }
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res;
        for (const auto& cur : pragmaDirectives) {
            res.push_back(cur.get());
        }
        for (const auto& cur : components) {
            res.push_back(cur.get());
        }
        for (const auto& cur : instantiations) {
            res.push_back(cur.get());
        }
        for (const auto& cur : functors) {
            res.push_back(cur.get());
        }
        for (const auto& cur : types) {
            res.push_back(cur.get());
        }
        for (const auto& cur : relations) {
            res.push_back(cur.get());
        }
        for (const auto& cur : clauses) {
            res.push_back(cur.get());
        }
        for (const auto& cur : ios) {
            res.push_back(cur.get());
        }
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        auto show = [&](auto&& xs, char const* sep = "\n") {
            if (!xs.empty()) os << join(xs, sep) << "\n";
        };

        show(pragmaDirectives, "\n\n");
        show(components);
        show(instantiations);
        show(types);
        show(functors);
        show(relations);
        show(clauses, "\n\n");
        show(ios, "\n\n");
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstProgram&>(node);
        if (!equal_targets(pragmaDirectives, other.pragmaDirectives)) {
            return false;
        }
        if (!equal_targets(components, other.components)) {
            return false;
        }
        if (!equal_targets(instantiations, other.instantiations)) {
            return false;
        }
        if (!equal_targets(functors, other.functors)) {
            return false;
        }
        if (!equal_targets(types, other.types)) {
            return false;
        }
        if (!equal_targets(relations, other.relations)) {
            return false;
        }
        if (!equal_targets(clauses, other.clauses)) {
            return false;
        }
        if (!equal_targets(ios, other.ios)) {
            return false;
        }
        return true;
    }

protected:
    friend class ComponentInstantiationTransformer;
    friend class ParserDriver;

    /* add type */
    void addType(Own<AstType> type) {
        assert(getType(*this, type->getQualifiedName()) == nullptr && "Redefinition of type!");
        types.push_back(std::move(type));
    }

    /** add IO directive */
    void addIO(Own<AstIO> directive) {
        assert(directive && "NULL IO directive");
        ios.push_back(std::move(directive));
    }

    /** add a pragma */
    void addPragma(Own<AstPragma> pragma) {
        assert(pragma && "NULL IO directive");
        pragmaDirectives.push_back(std::move(pragma));
    }

    /** add functor */
    void addFunctorDeclaration(Own<souffle::AstFunctorDeclaration> f) {
        assert(getFunctorDeclaration(*this, f->getName()) == nullptr && "Redefinition of functor!");
        functors.push_back(std::move(f));
    }

    /** add component */
    void addComponent(Own<AstComponent> c) {
        components.push_back(std::move(c));
    }

    /** add component instantiation */
    void addInstantiation(Own<AstComponentInit> i) {
        instantiations.push_back(std::move(i));
    }

    /** Program types  */
    VecOwn<AstType> types;

    /** Program relations */
    VecOwn<AstRelation> relations;

    /** External Functors */
    VecOwn<AstFunctorDeclaration> functors;

    /** Program clauses */
    VecOwn<AstClause> clauses;

    /** IO statements */
    VecOwn<AstIO> ios;

    /** Program components */
    VecOwn<AstComponent> components;

    /** Component instantiations */
    VecOwn<AstComponentInit> instantiations;

    /** Pragmas */
    VecOwn<AstPragma> pragmaDirectives;
};

}  // namespace souffle
