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

#include "SrcLocation.h"
#include "ast/AstClause.h"
#include "ast/AstIO.h"
#include "ast/AstNode.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstType.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <memory>
#include <ostream>
#include <set>
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
    AstComponentType(std::string name = "", std::vector<AstQualifiedName> params = {}, SrcLocation loc = {})
            : AstNode(std::move(loc)), name(std::move(name)), typeParams(std::move(params)) {}

    /** get component name */
    const std::string& getName() const {
        return name;
    }

    /** set component name */
    void setName(std::string n) {
        name = std::move(n);
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
        return new AstComponentType(name, typeParams, getSrcLoc());
    }

protected:
    void print(std::ostream& os) const override {
        os << name;
        if (!typeParams.empty()) {
            os << "<" << join(typeParams) << ">";
        }
    }

    bool equal(const AstNode& node) const override {
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
    AstComponentInit(std::string name, Own<AstComponentType> type, SrcLocation loc = {})
            : AstNode(std::move(loc)), instanceName(std::move(name)), componentType(std::move(type)) {}

    /** get instance name */
    const std::string& getInstanceName() const {
        return instanceName;
    }

    /** set instance name */
    void setInstanceName(std::string name) {
        instanceName = std::move(name);
    }

    /** get component type */
    const AstComponentType* getComponentType() const {
        return componentType.get();
    }

    /** set component type */
    void setComponentType(Own<AstComponentType> type) {
        componentType = std::move(type);
    }

    AstComponentInit* clone() const override {
        return new AstComponentInit(instanceName, souffle::clone(componentType), getSrcLoc());
    }

    void apply(const AstNodeMapper& mapper) override {
        componentType = mapper(std::move(componentType));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        return {componentType.get()};
    }

protected:
    void print(std::ostream& os) const override {
        os << ".init " << instanceName << " = " << *componentType;
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstComponentInit&>(node);
        return instanceName == other.instanceName && *componentType == *other.componentType;
    }

    /** instance name */
    std::string instanceName;

    /** actual component arguments for instantiation */
    Own<AstComponentType> componentType;
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
    void setComponentType(Own<AstComponentType> other) {
        componentType = std::move(other);
    }

    /** get base components */
    const std::vector<AstComponentType*> getBaseComponents() const {
        return toPtrVector(baseComponents);
    }

    /** add base components */
    void addBaseComponent(Own<AstComponentType> component) {
        baseComponents.push_back(std::move(component));
    }

    /** add type */
    void addType(Own<AstType> t) {
        types.push_back(std::move(t));
    }

    /** get types */
    std::vector<AstType*> getTypes() const {
        return toPtrVector(types);
    }

    /** copy base components */
    void copyBaseComponents(const AstComponent& other) {
        baseComponents = souffle::clone(other.baseComponents);
    }

    /** add relation */
    void addRelation(Own<AstRelation> r) {
        relations.push_back(std::move(r));
    }

    /** get relations */
    std::vector<AstRelation*> getRelations() const {
        return toPtrVector(relations);
    }

    /** add clause */
    void addClause(Own<AstClause> c) {
        clauses.push_back(std::move(c));
    }

    /** get clauses */
    std::vector<AstClause*> getClauses() const {
        return toPtrVector(clauses);
    }

    /** add IO */
    void addIO(Own<AstIO> directive) {
        ios.push_back(std::move(directive));
    }

    /** get IO statements */
    std::vector<AstIO*> getIOs() const {
        return toPtrVector(ios);
    }

    /** add components */
    void addComponent(Own<AstComponent> c) {
        components.push_back(std::move(c));
    }

    /** get components */
    std::vector<AstComponent*> getComponents() const {
        return toPtrVector(components);
    }

    /** add instantiation */
    void addInstantiation(Own<AstComponentInit> i) {
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
        res->componentType = souffle::clone(componentType);
        res->baseComponents = souffle::clone(baseComponents);
        res->components = souffle::clone(components);
        res->instantiations = souffle::clone(instantiations);
        res->types = souffle::clone(types);
        res->relations = souffle::clone(relations);
        res->clauses = souffle::clone(clauses);
        res->ios = souffle::clone(ios);
        res->overrideRules = overrideRules;
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
        show(overrideRules, ",", ".override ");
        show(clauses, "\n\n");
        show(ios, "\n\n");
        os << "}\n";
    }

    bool equal(const AstNode& node) const override {
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
    Own<AstComponentType> componentType;

    /** base components of component */
    VecOwn<AstComponentType> baseComponents;

    /** types declarations */
    VecOwn<AstType> types;

    /** relations */
    VecOwn<AstRelation> relations;

    /** clauses */
    VecOwn<AstClause> clauses;

    /** I/O directives */
    VecOwn<AstIO> ios;

    /** nested components */
    VecOwn<AstComponent> components;

    /** nested component instantiations. */
    VecOwn<AstComponentInit> instantiations;

    /** clauses of relations that are overwritten by this component */
    std::set<std::string> overrideRules;
};

}  // end of namespace souffle
