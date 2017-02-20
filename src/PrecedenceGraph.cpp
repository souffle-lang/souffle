/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PrecedenceGraph.cpp
 *
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "PrecedenceGraph.h"
#include "AstClause.h"
#include "AstRelation.h"
#include "AstUtils.h"
#include "AstVisitor.h"

#include <algorithm>
#include <iostream>
#include <set>

namespace souffle {

void PrecedenceGraph::run(const AstTranslationUnit& translationUnit) {
    // Get relations
    std::vector<AstRelation*> relations = translationUnit.getProgram()->getRelations();

    for (AstRelation* r : relations) {
        precedenceGraph.insertVertex(r);
        for (size_t i = 0; i < r->clauseSize(); i++) {
            AstClause* c = r->getClause(i);
            const std::set<const AstRelation*>& dependencies =
                    getBodyRelations(c, translationUnit.getProgram());
            for (std::set<const AstRelation*>::const_iterator irs = dependencies.begin();
                    irs != dependencies.end(); ++irs) {
                const AstRelation* source = (*irs);
                precedenceGraph.insertEdge(r, source);
            }
        }
    }
}

void RedundantRelations::run(const AstTranslationUnit& translationUnit) {
    precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();

    std::set<const AstRelation*> work;
    std::set<const AstRelation*> notRedundant;

    const std::vector<AstRelation*>& relations = translationUnit.getProgram()->getRelations();

    // Add all output relations to the work set
    for (const AstRelation* r : relations) {
        if (r->isComputed()) {
            work.insert(r);
        }
    }

    // Find all relations which are not redundant for the computations of the output relations.
    while (!work.empty()) {
        // Chose one element in the work set and add it to notRedundant
        const AstRelation* u = *(work.begin());
        work.erase(work.begin());
        notRedundant.insert(u);

        // Find all predecessors of u and add them to the worklist if they are not in the set notRedundant
        for (const AstRelation* predecessor : precedenceGraph->getPredecessors(u)) {
            if (!notRedundant.count(predecessor)) {
                work.insert(predecessor);
            }
        }
    }

    // All remaining relations are redundant.
    redundantRelations.clear();
    for (const AstRelation* r : relations) {
        if (!notRedundant.count(r)) {
            redundantRelations.insert(r);
        }
    }
}

void RecursiveClauses::run(const AstTranslationUnit& translationUnit) {
    visitDepthFirst(*translationUnit.getProgram(), [&](const AstClause& clause) {
        if (computeIsRecursive(clause, translationUnit)) {
            recursiveClauses.insert(&clause);
        }
    });
}

bool RecursiveClauses::computeIsRecursive(
        const AstClause& clause, const AstTranslationUnit& translationUnit) const {
    const AstProgram& program = *translationUnit.getProgram();

    // we want to reach the atom of the head through the body
    const AstRelation* trg = getHeadRelation(&clause, &program);

    std::set<const AstRelation*> reached;
    std::vector<const AstRelation*> worklist;

    // set up start list
    for (const AstAtom* cur : clause.getAtoms()) {
        auto rel = program.getRelation(cur->getName());
        if (rel == trg) {
            return true;
        }
        worklist.push_back(rel);
    }

    // process remaining elements
    while (!worklist.empty()) {
        // get next to process
        const AstRelation* cur = worklist.back();
        worklist.pop_back();

        // skip null pointers (errors in the input code)
        if (!cur) {
            continue;
        }

        // check whether this one has been checked before
        if (!reached.insert(cur).second) {
            continue;
        }

        // check all atoms in the relations
        for (const AstClause* cl : cur->getClauses()) {
            for (const AstAtom* at : cl->getAtoms()) {
                auto rel = program.getRelation(at->getName());
                if (rel == trg) {
                    return true;
                }
                worklist.push_back(rel);
            }
        }
    }

    // no cycles found
    return false;
}

void RelationSchedule::run(const AstTranslationUnit& translationUnit) {
    topsortSCCGraph = translationUnit.getAnalysis<TopologicallySortedSCCGraph>();
    precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();

    int numSCCs = topsortSCCGraph->getSCCGraph()->getGraph().vertexCount();
    std::vector<std::set<const AstRelation*>> relationExpirySchedule =
            computeRelationExpirySchedule(translationUnit);

    schedule.clear();
    for (int i = 0; i < numSCCs; i++) {
        int scc = topsortSCCGraph->getSCCOrder()[i];
        const std::set<const AstRelation*> computedRelations =
                topsortSCCGraph->getSCCGraph()->getGraph().table().get(scc);
        schedule.emplace_back(computedRelations, relationExpirySchedule[i],
                topsortSCCGraph->getSCCGraph()->isRecursive(scc));
    }
}

std::vector<std::set<const AstRelation*>> RelationSchedule::computeRelationExpirySchedule(
        const AstTranslationUnit& translationUnit) {
    std::vector<std::set<const AstRelation*>> relationExpirySchedule;
    // Compute for each step in the reverse topological order of evaluating the SCC the set of alive
    // relations.

    int numSCCs = topsortSCCGraph->getSCCOrder().size();

    // Alive set for each step
    std::vector<std::set<const AstRelation*>> alive(numSCCs);
    // Resize expired relations sets
    relationExpirySchedule.resize(numSCCs);

    // Mark the output relations as alive in the first step
    for (const AstRelation* relation : translationUnit.getProgram()->getRelations()) {
        if (relation->isComputed()) {
            alive[0].insert(relation);
        }
    }

    // Compute all alive relations by iterating over all steps in reverse order determine the dependencies
    for (int orderedSCC = 1; orderedSCC < numSCCs; orderedSCC++) {
        // Add alive set of previous step
        alive[orderedSCC].insert(alive[orderedSCC - 1].begin(), alive[orderedSCC - 1].end());

        // Add predecessors of relations computed in this step
        int scc = topsortSCCGraph->getSCCOrder()[numSCCs - orderedSCC];
        for (const AstRelation* r : topsortSCCGraph->getSCCGraph()->getGraph().table().get(scc)) {
            for (const AstRelation* predecessor : precedenceGraph->getPredecessors(r)) {
                alive[orderedSCC].insert(predecessor);
            }
        }

        // Compute expired relations in reverse topological order using the set difference of the alive sets
        // between steps.
        std::set_difference(alive[orderedSCC].begin(), alive[orderedSCC].end(), alive[orderedSCC - 1].begin(),
                alive[orderedSCC - 1].end(), std::inserter(relationExpirySchedule[numSCCs - orderedSCC],
                                                     relationExpirySchedule[numSCCs - orderedSCC].end()));
    }

    return relationExpirySchedule;
}

}  // end of namespace souffle
