/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Component.h
 *
 * Defines the component class
 *
 ***********************************************************************/

#pragma once

#include "ast/Clause.h"
#include "ast/ComponentInit.h"
#include "ast/ComponentType.h"
#include "ast/Directive.h"
#include "ast/Node.h"
#include "ast/Relation.h"
#include "ast/Type.h"
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast {

/**
 * @class Component
 * @brief Component class
 *
 * Example:
 *   .comp X = {
 *      .decl A(y:number)
 *      A(1).
 *   }
 *
 * Component consists of type declaration, relations, rules, etc.
 */
class Component : public Node {
public:
    /** Get component type */
    const ComponentType* getComponentType() const {
        return componentType.get();
    }

    /** Set component type */
    void setComponentType(Own<ComponentType> other);

    /** Get base components */
    const std::vector<ComponentType*> getBaseComponents() const;

    /** Add base components */
    void addBaseComponent(Own<ComponentType> component);

    /** Add type */
    void addType(Own<Type> t);

    /** Get types */
    std::vector<Type*> getTypes() const;

    /** Copy base components */
    void copyBaseComponents(const Component& other);

    /** Add relation */
    void addRelation(Own<Relation> r);

    /** Get relations */
    std::vector<Relation*> getRelations() const;

    /** Add clause */
    void addClause(Own<Clause> c);

    /** Get clauses */
    std::vector<Clause*> getClauses() const;

    /** Add directive */
    void addDirective(Own<Directive> directive);

    /** Get directive statements */
    std::vector<Directive*> getDirectives() const;

    /** Add components */
    void addComponent(Own<Component> c);

    /** Get components */
    std::vector<Component*> getComponents() const;

    /** Add instantiation */
    void addInstantiation(Own<ComponentInit> i);

    /** Get instantiation */
    std::vector<ComponentInit*> getInstantiations() const;

    /** Add override */
    void addOverride(const std::string& name) {
        overrideRules.insert(name);
    }

    /** Get override */
    const std::set<std::string>& getOverridden() const {
        return overrideRules;
    }

    void apply(const NodeMapper& mapper) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    Component* cloning() const override;

private:
    /** Name of component and its formal component arguments. */
    Own<ComponentType> componentType;

    /** Base components of component */
    VecOwn<ComponentType> baseComponents;

    /** Types declarations */
    VecOwn<Type> types;

    /** Relations */
    VecOwn<Relation> relations;

    /** Clauses */
    VecOwn<Clause> clauses;

    /** I/O directives */
    VecOwn<Directive> directives;

    /** Nested components */
    VecOwn<Component> components;

    /** Nested component instantiations. */
    VecOwn<ComponentInit> instantiations;

    /** Clauses of relations that are overwritten by this component */
    std::set<std::string> overrideRules;
};

}  // namespace souffle::ast
