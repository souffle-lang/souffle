/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstComponent.h
 *
 * Defines the class utilized to model a component within the input program.
 *
 ***********************************************************************/

#pragma once

#include "AstClause.h"
#include "AstIO.h"
#include "AstNode.h"
#include "AstRelation.h"
#include "AstType.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * A component type is
 *
 *                  name < Type1, Type2, ... >
 *
 * where name is the component name and < Type, Type, ... > is a
 * list of component type parameters (either actual or formal).
 */
class AstComponentType : public AstNode {
public:
    AstComponentType(
            std::string name = "", std::vector<AstQualifiedName> params = std::vector<AstQualifiedName>())
            : name(std::move(name)), typeParams(std::move(params)) {}

    /** get component name */
    const std::string& getName() const {
        return name;
    }

    /** set component name */
    void setName(const std::string& n) {
        name = n;
    }

    /** get component type parameters */
    const std::vector<AstQualifiedName>& getTypeParameters() const {
        return typeParams;
    }

    /** set component type parameters */
    void setTypeParameters(const std::vector<AstQualifiedName>& params) {
        typeParams = params;
    }

    AstComponentType* clone() const override {
        auto* res = new AstComponentType(name, typeParams);
        res->setSrcLoc(getSrcLoc());
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << name;
        if (!typeParams.empty()) {
            os << "<" << join(typeParams, ",") << ">";
        }
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstComponentType*>(&node));
        const auto& other = static_cast<const AstComponentType&>(node);
        return name == other.name && typeParams == other.typeParams;
    }

private:
    /** component name */
    std::string name;

    /** component type parameters */
    std::vector<AstQualifiedName> typeParams;
};

/**
 * Component intialization
 */
class AstComponentInit : public AstNode {
public:
    /** get instance name */
    const std::string& getInstanceName() const {
        return instanceName;
    }

    /** set instance name */
    void setInstanceName(const std::string& name) {
        instanceName = name;
    }

    /** get component type */
    const AstComponentType* getComponentType() const {
        return componentType.get();
    }

    /** set component type */
    void setComponentType(std::unique_ptr<AstComponentType> type) {
        componentType = std::move(type);
    }

    AstComponentInit* clone() const override {
        auto res = new AstComponentInit();
        res->setComponentType(std::unique_ptr<AstComponentType>(componentType->clone()));
        res->setInstanceName(instanceName);
        return res;
    }

    void apply(const AstNodeMapper& mapper) override {
        componentType = mapper(std::move(componentType));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res;
        res.push_back(componentType.get());
        return res;
    }

protected:
    void print(std::ostream& os) const override {
        os << ".init " << instanceName << " = " << *componentType;
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstComponentInit*>(&node));
        const auto& other = static_cast<const AstComponentInit&>(node);
        return instanceName == other.instanceName && *componentType == *other.componentType;
    }

    /** instance name */
    std::string instanceName;

    /** actual component arguments for instantiation */
    std::unique_ptr<AstComponentType> componentType;
};

/**
 * A AST node describing a component within the input program.
 */
class AstComponent : public AstNode {
public:
    /** get component type */
    const AstComponentType* getComponentType() const {
        return componentType.get();
    }

    /** set component type */
    void setComponentType(std::unique_ptr<AstComponentType> other) {
        componentType = std::move(other);
    }

    /** get base components */
    const std::vector<AstComponentType*> getBaseComponents() const {
        return toPtrVector(baseComponents);
    }

    /** add base components */
    void addBaseComponent(std::unique_ptr<AstComponentType> component) {
        baseComponents.push_back(std::move(component));
    }

    /** add type */
    void addType(std::unique_ptr<AstType> t) {
        types.push_back(std::move(t));
    }

    /** get types */
    std::vector<AstType*> getTypes() const {
        return toPtrVector(types);
    }

    /** copy base components */
    void copyBaseComponents(const AstComponent* other) {
        baseComponents.clear();
        for (const auto& baseComponent : other->getBaseComponents()) {
            baseComponents.emplace_back(baseComponent->clone());
        }
    }

    /** add relation */
    void addRelation(std::unique_ptr<AstRelation> r) {
        relations.push_back(std::move(r));
    }

    /** get relations */
    std::vector<AstRelation*> getRelations() const {
        return toPtrVector(relations);
    }

    /** add clause */
    void addClause(std::unique_ptr<AstClause> c) {
        clauses.push_back(std::move(c));
    }

    /** get clauses */
    std::vector<AstClause*> getClauses() const {
        return toPtrVector(clauses);
    }

    /** add IO */
    void addIO(std::unique_ptr<AstIO> directive) {
        ios.push_back(std::move(directive));
    }

    /** get IO statements */
    std::vector<AstIO*> getIOs() const {
        return toPtrVector(ios);
    }

    /** add components */
    void addComponent(std::unique_ptr<AstComponent> c) {
        components.push_back(std::move(c));
    }

    /** get components */
    std::vector<AstComponent*> getComponents() const {
        return toPtrVector(components);
    }

    /** add instantiation */
    void addInstantiation(std::unique_ptr<AstComponentInit> i) {
        instantiations.push_back(std::move(i));
    }

    /** get instantiation */
    std::vector<AstComponentInit*> getInstantiations() const {
        return toPtrVector(instantiations);
    }

    /** add override */
    void addOverride(const std::string& name) {
        overrideRules.insert(name);
    }

    /** get override */
    const std::set<std::string>& getOverridden() const {
        return overrideRules;
    }

    AstComponent* clone() const override {
        auto* res = new AstComponent();

        res->setComponentType(std::unique_ptr<AstComponentType>(componentType->clone()));
        for (const auto& cur : baseComponents) {
            res->baseComponents.emplace_back(cur->clone());
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
        for (const auto& cur : relations) {
            res->relations.emplace_back(cur->clone());
        }
        for (const auto& cur : clauses) {
            res->clauses.emplace_back(cur->clone());
        }
        for (const auto& cur : ios) {
            res->ios.emplace_back(cur->clone());
        }
        for (const auto& cur : overrideRules) {
            res->overrideRules.insert(cur);
        }

        return res;
    }

    void apply(const AstNodeMapper& mapper) override {
        componentType = mapper(std::move(componentType));
        for (auto& cur : baseComponents) {
            cur = mapper(std::move(cur));
        }
        for (auto& cur : components) {
            cur = mapper(std::move(cur));
        }
        for (auto& cur : instantiations) {
            cur = mapper(std::move(cur));
        }
        for (auto& cur : types) {
            cur = mapper(std::move(cur));
        }
        for (auto& cur : relations) {
            cur = mapper(std::move(cur));
        }
        for (auto& cur : clauses) {
            cur = mapper(std::move(cur));
        }
        for (auto& cur : ios) {
            cur = mapper(std::move(cur));
        }
    }

    std::vector<const AstNode*> getChildNodes() const override {
        std::vector<const AstNode*> res;

        res.push_back(componentType.get());
        for (const auto& cur : baseComponents) {
            res.push_back(cur.get());
        }
        for (const auto& cur : components) {
            res.push_back(cur.get());
        }
        for (const auto& cur : instantiations) {
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
        os << ".comp " << *componentType << " ";
        if (!baseComponents.empty()) {
            os << ": " << join(baseComponents, ",", print_deref<std::unique_ptr<AstComponentType>>()) << " ";
        }
        os << "{\n";
        if (!components.empty()) {
            os << join(components, "\n", print_deref<std::unique_ptr<AstComponent>>()) << "\n";
        }
        if (!instantiations.empty()) {
            os << join(instantiations, "\n", print_deref<std::unique_ptr<AstComponentInit>>()) << "\n";
        }
        if (!types.empty()) {
            os << join(types, "\n", print_deref<std::unique_ptr<AstType>>()) << "\n";
        }
        if (!relations.empty()) {
            os << join(relations, "\n", print_deref<std::unique_ptr<AstRelation>>()) << "\n";
        }
        if (!overrideRules.empty()) {
            os << ".override ";
            os << join(overrideRules, ",") << "\n";
        }
        if (!clauses.empty()) {
            os << join(clauses, "\n\n", print_deref<std::unique_ptr<AstClause>>()) << "\n";
        }
        if (!ios.empty()) {
            os << join(ios, "\n\n", print_deref<std::unique_ptr<AstIO>>()) << "\n";
        }

        os << "}\n";
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstComponent*>(&node));
        const auto& other = static_cast<const AstComponent&>(node);

        if (equal_ptr(componentType, other.componentType)) {
            return true;
        }
        if (!equal_targets(baseComponents, other.baseComponents)) {
            return false;
        }
        if (!equal_targets(components, other.components)) {
            return false;
        }
        if (!equal_targets(instantiations, other.instantiations)) {
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
        if (overrideRules != other.overrideRules) {
            return false;
        }
        return true;
    }

    /** name of component and its formal component arguments. */
    std::unique_ptr<AstComponentType> componentType;

    /** base components of component */
    std::vector<std::unique_ptr<AstComponentType>> baseComponents;

    /** types declarations */
    std::vector<std::unique_ptr<AstType>> types;

    /** relations */
    std::vector<std::unique_ptr<AstRelation>> relations;

    /** clauses */
    std::vector<std::unique_ptr<AstClause>> clauses;

    /** I/O directives */
    std::vector<std::unique_ptr<AstIO>> ios;

    /** nested components */
    std::vector<std::unique_ptr<AstComponent>> components;

    /** nested component instantiations. */
    std::vector<std::unique_ptr<AstComponentInit>> instantiations;

    /** clauses of relations that are overwritten by this component */
    std::set<std::string> overrideRules;
};

}  // end of namespace souffle
