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

#include "AstComponent.h"
#include "AstFunctorDeclaration.h"
#include "AstIO.h"
#include "AstNode.h"
#include "AstPragma.h"
#include "AstQualifiedName.h"
#include "AstRelation.h"
#include "AstType.h"
#include "AstUtils.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstClause;
class AstRelation;
class AstIO;

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
    const std::vector<std::unique_ptr<AstPragma>>& getPragmaDirectives() const {
        return pragmaDirectives;
    }

    /* add relation */
    void addRelation(std::unique_ptr<AstRelation> r) {
        assert(getRelation(*this, r->getQualifiedName()) == nullptr && "Redefinition of relation!");
        relations.push_back(std::move(r));
    }

    /** remove relation */
    bool removeRelation(const AstQualifiedName& name) {
        for (auto it = relations.begin(); it != relations.end(); it++) {
            const auto& rel = *it;
            if (rel->getQualifiedName() == name) {
                removeRelationClauses(*this, name);
                relations.erase(it);
                return true;
            }
        }
        return false;
    }

    /** add a clause */
    void addClause(std::unique_ptr<AstClause> clause) {
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

        for (const auto& cur : pragmaDirectives) {
            res->pragmaDirectives.emplace_back(cur->clone());
        }
        for (const auto& cur : components) {
            res->components.emplace_back(cur->clone());
        }
        for (const auto& cur : instantiations) {
            res->instantiations.emplace_back(cur->clone());
        }
        for (const auto& cur : types) {
            res->types.emplace_back(cur->clone());
        }
        for (const auto& cur : functors) {
            res->functors.emplace_back(cur->clone());
        }
        for (const auto& cur : relations) {
            res->relations.emplace_back(cur->clone());
        }
        for (const auto& cur : clauses) {
            res->clauses.emplace_back(cur->clone());
        }
        for (const auto& cur : ios) {
            res->ios.emplace_back(cur->clone());
        }

        // done
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
        if (!pragmaDirectives.empty()) {
            os << join(pragmaDirectives, "\n\n", print_deref<std::unique_ptr<AstPragma>>()) << "\n";
        }
        if (!components.empty()) {
            os << join(components, "\n", print_deref<std::unique_ptr<AstComponent>>()) << "\n";
        }
        if (!instantiations.empty()) {
            os << join(instantiations, "\n", print_deref<std::unique_ptr<AstComponentInit>>()) << "\n";
        }
        if (!types.empty()) {
            os << join(types, "\n", print_deref<std::unique_ptr<AstType>>()) << "\n";
        }
        if (!functors.empty()) {
            os << join(functors, "\n", print_deref<std::unique_ptr<AstFunctorDeclaration>>()) << "\n";
        }
        if (!relations.empty()) {
            os << join(relations, "\n", print_deref<std::unique_ptr<AstRelation>>()) << "\n";
        }
        if (!clauses.empty()) {
            os << join(clauses, "\n\n", print_deref<std::unique_ptr<AstClause>>()) << "\n";
        }
        if (!ios.empty()) {
            os << join(ios, "\n\n", print_deref<std::unique_ptr<AstIO>>()) << "\n";
        }
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstProgram*>(&node));
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
    void addType(std::unique_ptr<AstType> type) {
        assert(getType(*this, type->getQualifiedName()) == nullptr && "Redefinition of type!");
        types.push_back(std::move(type));
    }

    /** add IO directive */
    void addIO(std::unique_ptr<AstIO> directive) {
        assert(directive && "NULL IO directive");
        ios.push_back(std::move(directive));
    }

    /** add a pragma */
    void addPragma(std::unique_ptr<AstPragma> pragma) {
        assert(pragma && "NULL IO directive");
        pragmaDirectives.push_back(std::move(pragma));
    }

    /** add functor */
    void addFunctorDeclaration(std::unique_ptr<souffle::AstFunctorDeclaration> f) {
        assert(getFunctorDeclaration(*this, f->getName()) == nullptr && "Redefinition of functor!");
        functors.push_back(std::move(f));
    }

    /** add component */
    void addComponent(std::unique_ptr<AstComponent> c) {
        components.push_back(std::move(c));
    }

    /** add component instantiation */
    void addInstantiation(std::unique_ptr<AstComponentInit> i) {
        instantiations.push_back(std::move(i));
    }

    /** Program types  */
    std::vector<std::unique_ptr<AstType>> types;

    /** Program relations */
    std::vector<std::unique_ptr<AstRelation>> relations;

    /** External Functors */
    std::vector<std::unique_ptr<AstFunctorDeclaration>> functors;

    /** Program clauses */
    std::vector<std::unique_ptr<AstClause>> clauses;

    /** IO statements */
    std::vector<std::unique_ptr<AstIO>> ios;

    /** Program components */
    std::vector<std::unique_ptr<AstComponent>> components;

    /** Component instantiations */
    std::vector<std::unique_ptr<AstComponentInit>> instantiations;

    /** Pragmas */
    std::vector<std::unique_ptr<AstPragma>> pragmaDirectives;
};

}  // namespace souffle
