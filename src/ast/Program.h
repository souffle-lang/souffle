/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Program.h
 *
 * Defines the program class
 *
 ***********************************************************************/

#pragma once

#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/Directive.h"
#include "ast/FunctorDeclaration.h"
#include "ast/Node.h"
#include "ast/Pragma.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/Type.h"
#include "souffle/utility/Visitor.h"
#include <iosfwd>
#include <vector>

namespace souffle {
class ParserDriver;

namespace ast {
class Program;

namespace transform {
class ComponentInstantiationTransformer;
}
}  // namespace ast

// very common case optimisation: visiting every `Clause`
template <typename F, typename = detail::visitor_enable_if_arg0<F, ast::Clause>>
void visit(ast::Program&, F&&);
template <typename F, typename = detail::visitor_enable_if_arg0<F, ast::Clause>>
void visit(ast::Program const&, F&&);
}  // namespace souffle

namespace souffle::ast {

/**
 * @class Program
 * @brief The program class consists of relations, clauses and types.
 */
class Program : public Node {
public:
    /** Return types */
    std::vector<Type*> getTypes() const;

    /** Return relations */
    std::vector<Relation*> getRelations() const;

    /** Return clauses */
    std::vector<Clause*> getClauses() const;

    /** Return functor declarations */
    std::vector<FunctorDeclaration*> getFunctorDeclarations() const;

    /** Return relation directives */
    std::vector<Directive*> getDirectives() const;

    /** Add relation directive */
    void addDirective(Own<Directive> directive);

    /** Return pragma directives */
    const VecOwn<Pragma>& getPragmaDirectives() const {
        return pragmas;
    }

    /* Add relation */
    void addRelation(Own<Relation> relation);

    /** Remove relation */
    bool removeRelationDecl(const QualifiedName& name);

    /** Set clauses */
    void setClauses(VecOwn<Clause> newClauses);

    /** Add a clause */
    void addClause(Own<Clause> clause);

    /** Add a type declaration */
    void addType(Own<Type> type);

    /** Remove a clause */
    bool removeClause(const Clause* clause);

    /** Remove a directive */
    bool removeDirective(const Directive* directive);

    /** Return components */
    std::vector<Component*> getComponents() const;

    /** Return component instantiation */
    std::vector<ComponentInit*> getComponentInstantiations() const;

    /** Remove components and components' instantiations */
    void clearComponents();

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

    friend class souffle::ParserDriver;

    void addPragma(Own<Pragma> pragma);

    void addFunctorDeclaration(Own<FunctorDeclaration> functor);

    /** Add component */
    void addComponent(Own<Component> component);

    /** Add component instantiation */
    void addInstantiation(Own<ComponentInit> instantiation);

private:
    bool equal(const Node& node) const override;

    Program* cloning() const override;

private:
    /** Program types  */
    VecOwn<Type> types;

    /** Program relations */
    VecOwn<Relation> relations;

    /** External Functors */
    VecOwn<FunctorDeclaration> functors;

    /** Program clauses */
    VecOwn<Clause> clauses;

    /** Directives */
    VecOwn<Directive> directives;

    /** Component definitions */
    VecOwn<Component> components;

    /** Component instantiations */
    VecOwn<ComponentInit> instantiations;

    /** Pragmas */
    VecOwn<Pragma> pragmas;

#ifndef NDEBUG
    // SANCHECK - used to assert that the set of clauses isn't mutated mid visit
    // (easy extra check b/c `visit` is specialised for `Program` and `Clause`s)
    mutable uint32_t clause_visit_in_progress = 0;
#endif

    template <typename F, typename>
    friend void souffle::visit(Program&, F&&);
    template <typename F, typename>
    friend void souffle::visit(Program const&, F&&);
};

}  // namespace souffle::ast

namespace souffle {
template <typename F, typename>
void visit(ast::Program& program, F&& go) {
    program.clause_visit_in_progress++;
    for (auto&& cl : program.clauses)
        go(*cl);
    program.clause_visit_in_progress--;
}

template <typename F, typename>
void visit(ast::Program const& program, F&& go) {
    program.clause_visit_in_progress++;
    for (auto&& cl : program.clauses)
        go(static_cast<ast::Clause const&>(*cl));
    program.clause_visit_in_progress--;
}
}  // namespace souffle
