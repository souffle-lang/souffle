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
#include "ast/analysis/RelationDetailCache.h"
#include "ast/analysis/Type.h"
#include "ast/analysis/TypeSystem.h"

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

std::vector<const Variable*> getVariables(const Node& root) {
    // simply collect the list of all variables by visiting all variables
    std::vector<const Variable*> vars;
    visit(root, [&](const Variable& var) { vars.push_back(&var); });
    return vars;
}

std::vector<const RecordInit*> getRecords(const Node& root) {
    // simply collect the list of all records by visiting all records
    std::vector<const RecordInit*> recs;
    visit(root, [&](const RecordInit& rec) { recs.push_back(&rec); });
    return recs;
}

std::vector<Clause*> getClauses(const Program& program, const QualifiedName& relationName) {
    std::vector<Clause*> clauses;
    for (Clause* clause : program.getClauses()) {
        if (clause->getHead()->getQualifiedName() == relationName) {
            clauses.push_back(clause);
        }
    }
    return clauses;
}

std::vector<Clause*> getClauses(const Program& program, const Relation& rel) {
    return getClauses(program, rel.getQualifiedName());
}

std::vector<Directive*> getDirectives(const Program& program, const QualifiedName& name) {
    std::vector<Directive*> directives;
    for (Directive* dir : program.getDirectives()) {
        if (dir->getQualifiedName() == name) {
            directives.push_back(dir);
        }
    }
    return directives;
}

Relation* getRelation(const Program& program, const QualifiedName& name) {
    return getIf(program.getRelations(), [&](const Relation* r) { return r->getQualifiedName() == name; });
}

FunctorDeclaration* getFunctorDeclaration(const Program& program, const std::string& name) {
    return getIf(program.getFunctorDeclarations(),
            [&](const FunctorDeclaration* r) { return r->getName() == name; });
}

void removeRelation(TranslationUnit& tu, const QualifiedName& name) {
    Program& program = tu.getProgram();
    if (getRelation(program, name) != nullptr) {
        removeRelationClauses(tu, name);
        removeRelationIOs(tu, name);
        program.removeRelationDecl(name);
    }
}

void removeRelationClauses(TranslationUnit& tu, const QualifiedName& name) {
    Program& program = tu.getProgram();
    const auto& relDetail = tu.getAnalysis<analysis::RelationDetailCacheAnalysis>();

    // Make copies of the clauses to avoid use-after-delete for equivalent clauses
    std::set<Own<Clause>> clausesToRemove;
    for (const auto* clause : relDetail.getClauses(name)) {
        clausesToRemove.insert(clone(clause));
    }
    for (const auto& clause : clausesToRemove) {
        program.removeClause(clause.get());
    }

    tu.invalidateAnalyses();
}

void removeRelationIOs(TranslationUnit& tu, const QualifiedName& name) {
    Program& program = tu.getProgram();
    for (const auto* directive : getDirectives(program, name)) {
        program.removeDirective(directive);
    }
}

const Relation* getAtomRelation(const Atom* atom, const Program* program) {
    return getRelation(*program, atom->getQualifiedName());
}

const Relation* getHeadRelation(const Clause* clause, const Program* program) {
    return getAtomRelation(clause->getHead(), program);
}

std::set<const Relation*> getBodyRelations(const Clause* clause, const Program* program) {
    std::set<const Relation*> bodyRelations;
    for (const auto& lit : clause->getBodyLiterals()) {
        visit(*lit, [&](const Atom& atom) { bodyRelations.insert(getAtomRelation(&atom, program)); });
    }
    for (const auto& arg : clause->getHead()->getArguments()) {
        visit(*arg, [&](const Atom& atom) { bodyRelations.insert(getAtomRelation(&atom, program)); });
    }
    return bodyRelations;
}

bool hasClauseWithNegatedRelation(const Relation* relation, const Relation* negRelation,
        const Program* program, const Literal*& foundLiteral) {
    for (const Clause* cl : getClauses(*program, *relation)) {
        for (const auto* neg : getBodyLiterals<Negation>(*cl)) {
            if (negRelation == getAtomRelation(neg->getAtom(), program)) {
                foundLiteral = neg;
                return true;
            }
        }
    }
    return false;
}

bool hasClauseWithAggregatedRelation(const Relation* relation, const Relation* aggRelation,
        const Program* program, const Literal*& foundLiteral) {
    for (const Clause* cl : getClauses(*program, *relation)) {
        bool hasAgg = false;
        visit(*cl, [&](const Aggregator& cur) {
            visit(cur, [&](const Atom& atom) {
                if (aggRelation == getAtomRelation(&atom, program)) {
                    foundLiteral = &atom;
                    hasAgg = true;
                }
            });
        });
        if (hasAgg) {
            return true;
        }
    }
    return false;
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

bool isDeltaRelation(const QualifiedName& name) {
    const auto& qualifiers = name.getQualifiers();
    if (qualifiers.empty()) {
        return false;
    }
    return isPrefix("@delta_", qualifiers[0]);
}

Own<Clause> cloneHead(const Clause& clause) {
    auto myClone = mk<Clause>(clone(clause.getHead()), clause.getSrcLoc());
    if (clause.getExecutionPlan() != nullptr) {
        myClone->setExecutionPlan(clone(clause.getExecutionPlan()));
    }
    return myClone;
}

std::vector<Atom*> reorderAtoms(const std::vector<Atom*>& atoms, const std::vector<unsigned int>& newOrder) {
    // Validate given order
    assert(newOrder.size() == atoms.size());
    std::vector<unsigned int> nopOrder;
    for (unsigned int i = 0; i < atoms.size(); i++) {
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

Clause* reorderAtoms(const Clause* clause, const std::vector<unsigned int>& newOrder) {
    // Find all atom positions
    std::vector<unsigned int> atomPositions;
    std::vector<Literal*> bodyLiterals = clause->getBodyLiterals();
    for (unsigned int i = 0; i < bodyLiterals.size(); i++) {
        if (isA<Atom>(bodyLiterals[i])) {
            atomPositions.push_back(i);
        }
    }

    // Validate given order
    assert(newOrder.size() == atomPositions.size());
    std::vector<unsigned int> nopOrder;
    for (unsigned int i = 0; i < atomPositions.size(); i++) {
        nopOrder.push_back(i);
    }
    assert(std::is_permutation(nopOrder.begin(), nopOrder.end(), newOrder.begin()));

    // Create a new clause with the given atom order, leaving the rest unchanged
    auto newClause = cloneHead(*clause);
    unsigned int currentAtom = 0;
    for (unsigned int currentLiteral = 0; currentLiteral < bodyLiterals.size(); currentLiteral++) {
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

bool renameAtoms(Node& node, const std::map<QualifiedName, QualifiedName>& oldToNew) {
    struct rename_atoms : public NodeMapper {
        mutable bool changed{false};
        const std::map<QualifiedName, QualifiedName>& oldToNew;
        rename_atoms(const std::map<QualifiedName, QualifiedName>& oldToNew) : oldToNew(oldToNew) {}
        Own<Node> operator()(Own<Node> node) const override {
            node->apply(*this);
            if (auto* atom = as<Atom>(node)) {
                if (contains(oldToNew, atom->getQualifiedName())) {
                    auto renamedAtom = clone(atom);
                    renamedAtom->setQualifiedName(oldToNew.at(atom->getQualifiedName()));
                    changed = true;
                    return renamedAtom;
                }
            }
            return node;
        }
    };
    rename_atoms update(oldToNew);
    node.apply(update);
    return update.changed;
}

}  // namespace souffle::ast
