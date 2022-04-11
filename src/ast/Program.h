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
#include "ast/utility/Utils.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/Visitor.h"
#include "souffle/utility/span.h"
#include <iosfwd>
#include <vector>

namespace souffle {
class ParserDriver;

namespace ast {
class Atom;
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
    // Requirements for storing top-level nodes:
    // 1) remove-by-identity needs to be at least `O(lg n)`
    // 2) traversal order must be stable (at least for pretty printing)
    //
    // This implies that top-level nodes need to be stored in a linked-set structure.
    // Linked for stable traversal, and set for `O(lg n)` remove-by-identity.
    //
    // Furthermore, lookup of `Relation`, `Clause`, and `Directive` by name is a
    // common operation.
    // This is handled by storing `Relation`s, `Clause`s, and `Directive`s in a
    // map keyed by the relation name. The remove-by-identity time is linear,
    // but the size is much smaller, and this allows lookup by name w/o needing
    // an analysis and avoids the issue of the analysis results becoming stale
    // after common manipulations (e.g. `Clause` insertion).
    //
    // The downside is that the name must not be changed after the
    // node is added to the program. This is sanchecked by assertions.
    struct RelationInfo {
        // A well formed program has exactly one declaration per relation.
        // TODO:  Reject duplicate relation declarations in `SemanticChecker` instead of in the parser.
        VecOwn<Relation> decls;
        VecOwn<Clause> clauses;
        VecOwn<Directive> directives;
    };

    using RelationInfoMap = std::map<QualifiedName, RelationInfo>;

    RelationInfoMap& getRelationInfo() {
        return relations;
    }

    RelationInfoMap const& getRelationInfo() const {
        return relations;
    }

    RelationInfo* getRelationInfo(QualifiedName const& name) {
        auto it = relations.find(name);
        return it == relations.end() ? nullptr : &it->second;
    }

    RelationInfo const* getRelationInfo(QualifiedName const& name) const {
        auto it = relations.find(name);
        return it == relations.end() ? nullptr : &it->second;
    }

    /** Return types */
    std::vector<Type*> getTypes() const;

    /** Return relations */
    std::vector<Relation*> getRelations() const;

    /** Returns the first `Relation` declartion for a given name, if any */
    Relation* getRelation(QualifiedName const&) const;
    Relation* getRelation(Atom const&) const;
    Relation* getRelation(Clause const&) const;
    Relation* getRelation(Directive const&) const;

    /**
     * Returns all `Relation` declartions for a given name.
     * If the program contains multiple declarations for a given name then it is malformed
     * and it is reasonable to only consider the first declations.
     */
    std::vector<Relation*> getRelationAll(QualifiedName const&) const;

    /** Return clauses */
    std::vector<Clause*> getClauses() const;

    /** Return clauses for a given relation */
    std::vector<Clause*> getClauses(QualifiedName const&) const;

    /** Return clauses for a given relation */
    auto getClauses(Relation const& r) const {
        return getClauses(r.getQualifiedName());
    }

    /** Return functor declarations */
    std::vector<FunctorDeclaration*> getFunctorDeclarations() const;

    /** Return relation directives */
    std::vector<Directive*> getDirectives() const;

    /** Return relation directives for a relation */
    std::vector<Directive*> getDirectives(QualifiedName const&) const;

    /** Return relation directives for a relation */
    auto getDirectives(Relation const& r) const {
        return getDirectives(r.getQualifiedName());
    }

    /** Add relation directive */
    void addDirective(Own<Directive> directive);

    /** Return pragma directives */
    const VecOwn<Pragma>& getPragmaDirectives() const {
        return pragmas;
    }

    /* Add relation */
    void addRelation(Own<Relation> relation);

    /**
     * Remove a relation entirely, including the declaration(s), clauses, and directives.
     * @return true IFF the relation was found and removed
     */
    bool removeRelation(QualifiedName const&);

    /**
     * Remove a relation by identity. The relation must be owned by the program.
     * It is not expected that there are useful cases where some are not owned, and it is often
     * symptomatic of a bug to try to remove a clause that isn't part of the program.
     */
    void removeRelation(Relation const&);

    /** Add a clause */
    void addClause(Own<Clause> clause);

    // Common case helper.
    void addClauses(VecOwn<Clause> clauses);

    /** Add a type declaration */
    void addType(Own<Type> type);

    /**
     * Remove a clause by identity. The clause must be owned by the program.
     * It is not expected that there are useful cases where some are not owned, and it is often
     * symptomatic of a bug to try to remove a clause that isn't part of the program.
     */
    void removeClause(const Clause&);

    /**
     * Remove multiple clauses by identity. All of these clauses must be owned by the program.
     * It is not expected that there are useful cases where some are not owned, and it is often
     * symptomatic of a bug to try to remove a clause that isn't part of the program.
     */
    void removeClauses(span<Clause const* const>);

    /**
     * Remove a directive by identity.
     * @return true IFF the directive was found and removed
     */
    void removeDirective(const Directive&);

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

    /** Program relation declartions, clauses, and directives */
    RelationInfoMap relations;

    /** External Functors */
    VecOwn<FunctorDeclaration> functors;

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
#ifndef NDEBUG
    program.clause_visit_in_progress++;
#endif
    for ([[maybe_unused]] auto&& [k, info] : program.relations)
        for (auto&& cl : info.clauses) {
            assert(k == ast::getName(*cl));
            go(*cl);
            // post-condition verify isn't robust; other `visit` calls could inadvertently modify head name
            // e.g. `visit(program, [](Atom&) {})`
            assert(k == ast::getName(*cl));
        }
#ifndef NDEBUG
    program.clause_visit_in_progress--;
#endif
}

template <typename F, typename>
void visit(ast::Program const& program, F&& go) {
#ifndef NDEBUG
    program.clause_visit_in_progress++;
#endif
    for ([[maybe_unused]] auto&& [k, info] : program.relations)
        for (auto&& cl : info.clauses) {
            assert(k == ast::getName(*cl));
            go(static_cast<ast::Clause const&>(*cl));
            // post-condition verify isn't robust; other `visit` calls could inadvertently modify head name
            // e.g. `visit(program, [](Atom&) {})`
            assert(k == ast::getName(*cl));
        }
#ifndef NDEBUG
    program.clause_visit_in_progress--;
#endif
}
}  // namespace souffle
