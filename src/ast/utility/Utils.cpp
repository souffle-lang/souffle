/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.cpp
 *
 * A collection of utilities operating on AST constructs.
 *
 ***********************************************************************/

#include "ast/utility/Utils.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Directive.h"
#include "ast/ExecutionPlan.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/Functor.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/analysis/typesystem/TypeSystem.h"

#include "ast/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <memory>

namespace souffle::ast {

std::string pprint(const Node& node) {
    return toString(node);
}

QualifiedName getName(const Atom& x) {
    return x.getQualifiedName();
}

QualifiedName getName(const Clause& x) {
    return x.getHead()->getQualifiedName();
}

QualifiedName getName(const Directive& x) {
    return x.getQualifiedName();
}

QualifiedName getName(const Relation& x) {
    return x.getQualifiedName();
}

FunctorDeclaration* getFunctorDeclaration(const Program& program, const std::string& name) {
    // FIXME: O(n). This is awful.
    return getIf(program.getFunctorDeclarations(),
            [&](const FunctorDeclaration* r) { return r->getName() == name; });
}

bool hasClauseWithNegatedRelation(const Relation* relation, const Relation* negRelation,
        const Program* program, const Literal*& foundLiteral) {
    for (auto&& cl : program->getClauses(*relation)) {
        for (const auto* neg : getBodyLiterals<Negation>(*cl)) {
            if (negRelation == program->getRelation(*neg->getAtom())) {
                foundLiteral = neg;
                return true;
            }
        }
    }
    return false;
}

bool hasClauseWithAggregatedRelation(const Relation* relation, const Relation* aggRelation,
        const Program* program, const Literal*& foundLiteral) {
    bool found_in_agg = false;
    visitFrontier(program->getClauses(*relation), [&](const Aggregator& cur) {
        found_in_agg = found_in_agg || visitExists(cur, [&](const Atom& atom) {
            if (aggRelation == program->getRelation(atom)) {
                foundLiteral = &atom;
                return true;
            }

            return false;
        });
        return found_in_agg;
    });

    return found_in_agg;
}

bool isRecursiveClause(const Clause& clause) {
    QualifiedName relationName = clause.getHead()->getQualifiedName();
    bool recursive = false;
    visit(clause.getBodyLiterals(), [&](const Atom& atom) {
        if (atom.getQualifiedName() == relationName) {
            recursive = true;
        }
    });
    return recursive;
}

bool isFact(const Clause& clause) {
    // there must be a head
    if (clause.getHead() == nullptr) {
        return false;
    }
    // there must not be any body clauses
    if (!clause.getBodyLiterals().empty()) {
        return false;
    }

    // and there are no aggregates
    bool hasAggregatesOrMultiResultFunctor = false;
    visit(*clause.getHead(), [&](const Argument& arg) {
        if (isA<Aggregator>(arg)) {
            hasAggregatesOrMultiResultFunctor = true;
        }

        auto* func = as<IntrinsicFunctor>(arg);
        hasAggregatesOrMultiResultFunctor |=
                (func != nullptr) && analysis::FunctorAnalysis::isMultiResult(*func);
    });
    return !hasAggregatesOrMultiResultFunctor;
}

bool isRule(const Clause& clause) {
    return (clause.getHead() != nullptr) && !isFact(clause);
}

bool isProposition(const Atom* atom) {
    return atom->getArguments().empty();
}

std::vector<Atom*> reorderAtoms(const std::vector<Atom*>& atoms, const std::vector<std::size_t>& newOrder) {
    // Validate given order
    assert(newOrder.size() == atoms.size());
    std::vector<std::size_t> nopOrder;
    for (std::size_t i = 0; i < atoms.size(); i++) {
        nopOrder.push_back(i);
    }
    assert(std::is_permutation(nopOrder.begin(), nopOrder.end(), newOrder.begin()));

    // Create the result
    std::vector<Atom*> result(atoms.size());
    for (std::size_t i = 0; i < atoms.size(); i++) {
        result[i] = atoms[newOrder[i]];
    }
    return result;
}

Clause* reorderAtoms(const Clause* clause, const std::vector<std::size_t>& newOrder) {
    // Find all atom positions
    std::vector<std::size_t> atomPositions;
    std::vector<Literal*> bodyLiterals = clause->getBodyLiterals();
    for (std::size_t i = 0; i < bodyLiterals.size(); i++) {
        if (isA<Atom>(bodyLiterals[i])) {
            atomPositions.push_back(i);
        }
    }

    // Validate given order
    assert(newOrder.size() == atomPositions.size());
    std::vector<std::size_t> nopOrder;
    for (std::size_t i = 0; i < atomPositions.size(); i++) {
        nopOrder.push_back(i);
    }
    assert(std::is_permutation(nopOrder.begin(), nopOrder.end(), newOrder.begin()));

    // Create a new clause with the given atom order, leaving the rest unchanged
    auto newClause = Own<Clause>(clause->cloneHead());
    std::size_t currentAtom = 0;
    for (std::size_t currentLiteral = 0; currentLiteral < bodyLiterals.size(); currentLiteral++) {
        Literal* literalToAdd = bodyLiterals[currentLiteral];
        if (isA<Atom>(literalToAdd)) {
            // Atoms should be reordered
            literalToAdd = bodyLiterals[atomPositions[newOrder[currentAtom++]]];
        }
        newClause->addToBody(clone(literalToAdd));
    }

    // FIXME: tomp - fix ownership
    return newClause.release();
}

void negateConstraintInPlace(Constraint& constraint) {
    if (auto* bcstr = as<BooleanConstraint>(constraint)) {
        bcstr->set(!bcstr->isTrue());
    } else if (auto* cstr = as<BinaryConstraint>(constraint)) {
        cstr->setBaseOperator(souffle::negatedConstraintOp(cstr->getBaseOperator()));
    } else {
        fatal("Unknown ast-constraint type");
    }
}

bool renameAtoms(Own<Clause>& clause, const std::map<QualifiedName, QualifiedName>& oldToNew) {
    bool changed = false;
    visit(clause, [&](Atom& atom) {
        auto it = oldToNew.find(atom.getQualifiedName());
        if (it != oldToNew.end()) {
            atom.setQualifiedName(it->second);
            changed = true;
        }
    });
    return changed;
}

bool renameAtoms(Program& program, const std::map<QualifiedName, QualifiedName>& oldToNew) {
    bool changed = false;

    // Can't rename clause head atoms while the clause is attached w/o invalidating by-name lookup tables.
    // -> Detach clauses w/ renamed head atoms. Reinsert after atom rename.
    VecOwn<Clause> renamed_clauses;
    for ([[maybe_unused]] auto&& [name, info] : program.getRelationInfo()) {
        bool clauses_changed = false;
        for (auto&& cl : info.clauses) {
            assert(getName(*cl) == name && "sanity check - name lookup tables corrupted");
            clauses_changed |= renameAtoms(cl, oldToNew);
        }

        if (contains(oldToNew, name)) {
            assert((clauses_changed || info.clauses.empty()) && "clause heads should have been renamed");
            renamed_clauses = concat(std::move(renamed_clauses), std::move(info.clauses));
            info.clauses = VecOwn<Clause>{};
        }

        changed |= clauses_changed;
    }

    // Reattach clauses after doing renames to prevent transitive renames.
    // This function only does a single renaming 'step'.
    for (auto& cl : renamed_clauses)
        program.addClause(std::move(cl));

    return changed;
}

bool renameAtoms(Program& program, QualifiedName const& relation,
        const std::map<QualifiedName, QualifiedName>& oldToNew) {
    auto info = program.getRelationInfo(relation);
    if (!info) return false;

    bool changed = false;
    for (auto& cl : info->clauses)
        changed |= renameAtoms(cl, oldToNew);

    // if clause heads were renamed -> detach/re-insert clauses to maintain by-name lookup tables
    if (contains(oldToNew, relation)) {
        assert(changed && "clause head atoms should have been renamed");

        for (auto& cl : info->clauses)
            program.addClause(std::move(cl));  // steal & reinsert under new name

        info->clauses.clear();
    }

    return changed;
}
}  // namespace souffle::ast
