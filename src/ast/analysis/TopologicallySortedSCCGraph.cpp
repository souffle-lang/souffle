/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TopologicallySortedSCCGraph.cpp
 *
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "ast/analysis/TopologicallySortedSCCGraph.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/analysis/SCCGraph.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <set>
#include <string>

namespace souffle {

int TopologicallySortedSCCGraphAnalysis::topologicalOrderingCost(
        const std::vector<size_t>& permutationOfSCCs) const {
    // create variables to hold the cost of the current SCC and the permutation as a whole
    int costOfSCC = 0;
    int costOfPermutation = -1;
    // obtain an iterator to the end of the already ordered partition of sccs
    auto it_k = permutationOfSCCs.begin() + sccOrder.size();
    // for each of the scc's in the ordering, resetting the cost of the scc to zero on each loop
    for (auto it_i = permutationOfSCCs.begin(); it_i != permutationOfSCCs.end(); ++it_i, costOfSCC = 0) {
        // if the index of the current scc is after the end of the ordered partition
        if (it_i >= it_k) {
            // check that the index of all predecessor sccs of are before the index of the current scc
            for (auto scc : sccGraph->getPredecessorSCCs(*it_i)) {
                if (std::find(permutationOfSCCs.begin(), it_i, scc) == it_i) {
                    // if not, the sort is not a valid topological sort
                    return -1;
                }
            }
        }
        // otherwise, calculate the cost of the current scc
        // as the number of sccs with an index before the current scc
        for (auto it_j = permutationOfSCCs.begin(); it_j != it_i; ++it_j) {
            // having some successor scc with an index after the current scc
            for (auto scc : sccGraph->getSuccessorSCCs(*it_j)) {
                if (std::find(permutationOfSCCs.begin(), it_i, scc) == it_i) {
                    costOfSCC++;
                }
            }
        }
        // and if this cost is greater than the maximum recorded cost for the whole permutation so far,
        // set the cost of the permutation to it
        if (costOfSCC > costOfPermutation) {
            costOfPermutation = costOfSCC;
        }
    }
    return costOfPermutation;
}

void TopologicallySortedSCCGraphAnalysis::computeTopologicalOrdering(size_t scc, std::vector<bool>& visited) {
    // create a flag to indicate that a successor was visited (by default it hasn't been)
    bool found = false;
    bool hasUnvisitedSuccessor = false;
    bool hasUnvisitedPredecessor = false;
    // for each successor of the input scc
    const auto& successorsToVisit = sccGraph->getSuccessorSCCs(scc);
    for (const auto scc_i : successorsToVisit) {
        if (visited[scc_i]) {
            continue;
        }
        hasUnvisitedPredecessor = false;
        const auto& successorsPredecessors = sccGraph->getPredecessorSCCs(scc_i);
        for (const auto scc_j : successorsPredecessors) {
            if (!visited[scc_j]) {
                hasUnvisitedPredecessor = true;
                break;
            }
        }
        if (!hasUnvisitedPredecessor) {
            // give it a temporary marking
            visited[scc_i] = true;
            // add it to the permanent ordering
            sccOrder.push_back(scc_i);
            // and use it as a root node in a recursive call to this function
            computeTopologicalOrdering(scc_i, visited);
            // finally, indicate that a successor has been found for this node
            found = true;
        }
    }
    // return at once if no valid successors have been found; as either it has none or they all have a
    // better predecessor
    if (!found) {
        return;
    }
    hasUnvisitedPredecessor = false;
    const auto& predecessors = sccGraph->getPredecessorSCCs(scc);
    for (const auto scc_j : predecessors) {
        if (!visited[scc_j]) {
            hasUnvisitedPredecessor = true;
            break;
        }
    }
    hasUnvisitedSuccessor = false;
    const auto& successors = sccGraph->getSuccessorSCCs(scc);
    for (const auto scc_j : successors) {
        if (!visited[scc_j]) {
            hasUnvisitedSuccessor = true;
            break;
        }
    }
    // otherwise, if more white successors remain for the current scc, use it again as the root node in a
    // recursive call to this function
    if (hasUnvisitedSuccessor && !hasUnvisitedPredecessor) {
        computeTopologicalOrdering(scc, visited);
    }
}

void TopologicallySortedSCCGraphAnalysis::run(const AstTranslationUnit& translationUnit) {
    // obtain the scc graph
    sccGraph = translationUnit.getAnalysis<SCCGraphAnalysis>();
    // clear the list of ordered sccs
    sccOrder.clear();
    std::vector<bool> visited;
    visited.resize(sccGraph->getNumberOfSCCs());
    std::fill(visited.begin(), visited.end(), false);
    // generate topological ordering using forwards algorithm (like Khan's algorithm)
    // for each of the sccs in the graph
    for (size_t scc = 0; scc < sccGraph->getNumberOfSCCs(); ++scc) {
        // if that scc has no predecessors
        if (sccGraph->getPredecessorSCCs(scc).empty()) {
            // put it in the ordering
            sccOrder.push_back(scc);
            visited[scc] = true;
            // if the scc has successors
            if (!sccGraph->getSuccessorSCCs(scc).empty()) {
                computeTopologicalOrdering(scc, visited);
            }
        }
    }
}

void TopologicallySortedSCCGraphAnalysis::print(std::ostream& os) const {
    os << "--- partial order of strata as list of pairs ---" << std::endl;
    for (size_t sccIndex = 0; sccIndex < sccOrder.size(); sccIndex++) {
        const auto& successorSccs = sccGraph->getSuccessorSCCs(sccOrder.at(sccIndex));
        // use a self-loop to indicate that an SCC has no successors or predecessors
        if (successorSccs.empty() && sccGraph->getPredecessorSCCs(sccOrder.at(sccIndex)).empty()) {
            os << sccIndex << " " << sccIndex << std::endl;
            continue;
        }
        for (const auto successorScc : successorSccs) {
            const auto successorSccIndex = *std::find(sccOrder.begin(), sccOrder.end(), successorScc);
            os << sccIndex << " " << successorSccIndex << std::endl;
        }
    }
    os << "--- total order with relations of each strata ---" << std::endl;
    for (size_t i = 0; i < sccOrder.size(); i++) {
        os << i << ": [";
        os << join(sccGraph->getInternalRelations(sccOrder[i]), ", ",
                [](std::ostream& out, const AstRelation* rel) { out << rel->getQualifiedName(); });
        os << "]" << std::endl;
    }
    os << std::endl;
    os << "--- statistics of topological order ---" << std::endl;
    os << "cost: " << topologicalOrderingCost(sccOrder) << std::endl;
}

}  // end of namespace souffle
