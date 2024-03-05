#pragma once

#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/Directive.h"
#include "ast/Lattice.h"
#include "ast/Relation.h"
#include "ast/Type.h"

namespace souffle::ast {
class Component;

/// Interface of an item container.
///
/// Items are program elements that can be found both at top-level and within a
/// component.
struct ItemContainer {
    virtual ~ItemContainer() = default;

    /** Add type */
    virtual void addType(Own<Type> t) = 0;

    /** Get types */
    virtual std::vector<Type*> getTypes() const = 0;

    /** Add relation */
    virtual void addRelation(Own<Relation> r) = 0;

    /** Get relations */
    virtual std::vector<Relation*> getRelations() const = 0;

    /** Add clause */
    virtual void addClause(Own<Clause> c) = 0;

    /** Get clauses */
    virtual std::vector<Clause*> getClauses() const = 0;

    /** Add directive */
    virtual void addDirective(Own<Directive> directive) = 0;

    /** Get directive statements */
    virtual std::vector<Directive*> getDirectives() const = 0;

    /** Add components */
    virtual void addComponent(Own<Component> c) = 0;

    /** Get components */
    virtual std::vector<Component*> getComponents() const = 0;

    /** Add instantiation */
    virtual void addInstantiation(Own<ComponentInit> i) = 0;

    /** Get instantiation */
    virtual std::vector<ComponentInit*> getInstantiations() const = 0;

    /** Add a lattice declaration */
    virtual void addLattice(Own<Lattice> lattice) = 0;

    /** Return lattices */
    virtual std::vector<Lattice*> getLattices() const = 0;
};
}  // namespace souffle::ast
