/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstUtils.cpp
 *
 * A collection of utilities operating on AST constructs.
 *
 ***********************************************************************/

#include "AstUtils.h"
#include "AstArgument.h"
#include "AstClause.h"
#include "AstLiteral.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstVisitor.h"

namespace souffle {

std::vector<const AstVariable*> getVariables(const AstNode& root) {
    // simply collect the list of all variables by visiting all variables
    std::vector<const AstVariable*> vars;
    visitDepthFirst(root, [&](const AstVariable& var) { vars.push_back(&var); });
    return vars;
}

std::vector<const AstRecordInit*> getRecords(const AstNode& root) {
    // simply collect the list of all records by visiting all records
    std::vector<const AstRecordInit*> recs;
    visitDepthFirst(root, [&](const AstRecordInit& rec) { recs.push_back(&rec); });
    return recs;
}

std::vector<const AstSumInit*> getSums(const AstNode& root) {
    // simply collect the list of all records by visiting all records
    std::vector<const AstSumInit*> sums;
    visitDepthFirst(root, [&](const AstSumInit& sum) { sums.push_back(&sum); });
    return sums;
}

const AstRelation* getAtomRelation(const AstAtom* atom, const AstProgram* program) {
    return program->getRelation(atom->getQualifiedName());
}

const AstRelation* getHeadRelation(const AstClause* clause, const AstProgram* program) {
    return getAtomRelation(clause->getHead(), program);
}

std::set<const AstRelation*> getBodyRelations(const AstClause* clause, const AstProgram* program) {
    std::set<const AstRelation*> bodyRelations;
    for (const auto& lit : clause->getBodyLiterals()) {
        visitDepthFirst(
                *lit, [&](const AstAtom& atom) { bodyRelations.insert(getAtomRelation(&atom, program)); });
    }
    for (const auto& arg : clause->getHead()->getArguments()) {
        visitDepthFirst(
                *arg, [&](const AstAtom& atom) { bodyRelations.insert(getAtomRelation(&atom, program)); });
    }
    return bodyRelations;
}

size_t getClauseNum(const AstProgram* program, const AstClause* clause) {
    // TODO (azreika): This number might change between the provenance transformer and the AST->RAM
    // translation. Might need a better way to assign IDs to clauses... (see PR #1288).
    const AstRelation* rel = program->getRelation(clause->getHead()->getQualifiedName());
    assert(rel != nullptr && "clause relation does not exist");

    size_t clauseNum = 1;
    for (const auto* cur : rel->getClauses()) {
        bool isFact = cur->getBodyLiterals().empty();
        if (cur == clause) {
            return isFact ? 0 : clauseNum;
        }

        if (!isFact) {
            clauseNum++;
        }
    }
    assert(false && "clause does not exist");
}

bool hasClauseWithNegatedRelation(const AstRelation* relation, const AstRelation* negRelation,
        const AstProgram* program, const AstLiteral*& foundLiteral) {
    for (const AstClause* cl : relation->getClauses()) {
        for (const auto* neg : getBodyLiterals<AstNegation>(*cl)) {
            if (negRelation == getAtomRelation(neg->getAtom(), program)) {
                foundLiteral = neg;
                return true;
            }
        }
    }
    return false;
}

bool hasClauseWithAggregatedRelation(const AstRelation* relation, const AstRelation* aggRelation,
        const AstProgram* program, const AstLiteral*& foundLiteral) {
    for (const AstClause* cl : relation->getClauses()) {
        bool hasAgg = false;
        visitDepthFirst(*cl, [&](const AstAggregator& cur) {
            visitDepthFirst(cur, [&](const AstAtom& atom) {
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

bool isRecursiveClause(const AstClause& clause) {
    AstQualifiedName relationName = clause.getHead()->getQualifiedName();
    bool recursive = false;
    visitDepthFirst(clause.getBodyLiterals(), [&](const AstAtom& atom) {
        if (atom.getQualifiedName() == relationName) {
            recursive = true;
        }
    });
    return recursive;
}

bool isFact(const AstClause& clause) {
    // there must be a head
    if (clause.getHead() == nullptr) {
        return false;
    }
    // there must not be any body clauses
    if (!clause.getBodyLiterals().empty()) {
        return false;
    }

    // and there are no aggregates
    bool hasAggregates = false;
    visitDepthFirst(*clause.getHead(), [&](const AstAggregator& cur) { hasAggregates = true; });
    return !hasAggregates;
}

bool isRule(const AstClause& clause) {
    return (clause.getHead() != nullptr) && !isFact(clause);
}

AstClause* cloneHead(const AstClause* clause) {
    auto* clone = new AstClause();
    clone->setSrcLoc(clause->getSrcLoc());
    clone->setHead(std::unique_ptr<AstAtom>(clause->getHead()->clone()));
    if (clause->getExecutionPlan() != nullptr) {
        clone->setExecutionPlan(std::unique_ptr<AstExecutionPlan>(clause->getExecutionPlan()->clone()));
    }
    return clone;
}

AstClause* reorderAtoms(const AstClause* clause, const std::vector<unsigned int>& newOrder) {
    // Find all atom positions
    std::vector<unsigned int> atomPositions;
    std::vector<AstLiteral*> bodyLiterals = clause->getBodyLiterals();
    for (unsigned int i = 0; i < bodyLiterals.size(); i++) {
        if (dynamic_cast<AstAtom*>(bodyLiterals[i]) != nullptr) {
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
    AstClause* newClause = cloneHead(clause);
    unsigned int currentAtom = 0;
    for (unsigned int currentLiteral = 0; currentLiteral < bodyLiterals.size(); currentLiteral++) {
        AstLiteral* literalToAdd = bodyLiterals[currentLiteral];
        if (dynamic_cast<AstAtom*>(literalToAdd) != nullptr) {
            // Atoms should be reordered
            literalToAdd = bodyLiterals[atomPositions[newOrder[currentAtom++]]];
        }
        newClause->addToBody(std::unique_ptr<AstLiteral>(literalToAdd->clone()));
    }

    return newClause;
}
}  // end of namespace souffle
