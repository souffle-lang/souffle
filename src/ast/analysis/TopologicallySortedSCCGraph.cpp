/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
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
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/SCCGraph.h"
#include "souffle/utility/StreamUtil.h"
#include <algorithm>
#include <set>
#include <string>

namespace souffle::ast::analysis {

TopologicallySortedSCCGraphAnalysis::TopologicallySortedSCCGraphAnalysis() : Analysis(name) {}

int TopologicallySortedSCCGraphAnalysis::topologicalOrderingCost(
        const std::vector<std::size_t>& permutationOfSCCs) const {
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

void TopologicallySortedSCCGraphAnalysis::computeTopologicalOrdering(
        std::size_t scc, std::vector<int>& visited) {
    if (visited[scc] >= 0) {
        return;
    }

    int maxDist = 0;
    const auto& predecessors = sccGraph->getPredecessorSCCs(scc);
    for (const auto pred : predecessors) {
        if (visited[pred] < 0) {
            // has an unvisited predecessor
            return;
        } else {
            maxDist = std::max(maxDist, visited[pred] + 1);
        }
    }

    visited[scc] = maxDist;
    sccDistance.emplace(scc, maxDist);
    sccOrder.emplace_back(scc);

    for (const auto succ : sccGraph->getSuccessorSCCs(scc)) {
        computeTopologicalOrdering(succ, visited);
    }
}

void TopologicallySortedSCCGraphAnalysis::run(const TranslationUnit& translationUnit) {
    // obtain the scc graph
    sccGraph = &translationUnit.getAnalysis<SCCGraphAnalysis>();

    // clear the list of ordered sccs
    sccOrder.clear();
    sccDistance.clear();

    // Compute the maximum distance from the root scc(s) to each scc.
    //
    // visited[scc] < 0 when the scc distance from the root(s) is not known yet
    // visited[scc] >= 0 is the maximum distance of the scc from a root
    //
    // this provides a partial order between sccs, several sccs may have the
    // same maximum distance from the roots.
    std::vector<int> visited;
    visited.resize(sccGraph->getNumberOfSCCs());
    std::fill(visited.begin(), visited.end(), -1);
    for (std::size_t scc = 0; scc < sccGraph->getNumberOfSCCs(); ++scc) {
        computeTopologicalOrdering(scc, visited);
    }

    // find the least relation qualified name of each scc, using the lexicographic order.
    std::vector<QualifiedName> sccLeastQN;
    sccLeastQN.resize(sccGraph->getNumberOfSCCs());
    for (std::size_t scc = 0; scc < sccGraph->getNumberOfSCCs(); ++scc) {
        sccLeastQN[scc] = (*sccGraph->getInternalRelations(scc).begin())->getQualifiedName();
    }

    // sort sccs by distance from roots and then by lexicographic order of the least
    // relation qualified name in each scc.
    //
    // this provides a deterministic total order between sccs.
    std::sort(sccOrder.begin(), sccOrder.end(), [&](std::size_t lhs, std::size_t rhs) {
        if (sccDistance[lhs] < sccDistance[rhs]) {
            return true;
        } else if (sccDistance[lhs] > sccDistance[rhs]) {
            return false;
        } else {
            return sccLeastQN[lhs].lexicalLess(sccLeastQN[rhs]);
        }
    });
}

void TopologicallySortedSCCGraphAnalysis::print(std::ostream& os) const {
    os << "--- partial order of strata as list of pairs ---" << std::endl;
    for (std::size_t sccIndex = 0; sccIndex < sccOrder.size(); sccIndex++) {
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
    for (std::size_t i = 0; i < sccOrder.size(); i++) {
        os << i << ": [";
        os << join(sccGraph->getInternalRelations(sccOrder[i]), ", ",
                [](std::ostream& out, const Relation* rel) { out << rel->getQualifiedName(); });
        os << "]" << std::endl;
    }
    os << std::endl;
    os << "--- statistics of topological order ---" << std::endl;
    os << "cost: " << topologicalOrderingCost(sccOrder) << std::endl;
}

const std::vector<std::size_t>& TopologicallySortedSCCGraphAnalysis::order() const {
    return sccOrder;
}

std::size_t TopologicallySortedSCCGraphAnalysis::sccOfIndex(const std::size_t index) const {
    return sccOrder.at(index);
}

std::size_t TopologicallySortedSCCGraphAnalysis::indexOfScc(const std::size_t scc) const {
    auto it = std::find(sccOrder.begin(), sccOrder.end(), scc);
    assert(it != sccOrder.end());
    return (std::size_t)std::distance(sccOrder.begin(), it);
}

std::set<std::size_t> TopologicallySortedSCCGraphAnalysis::indexOfScc(
        const std::set<std::size_t>& sccs) const {
    std::set<std::size_t> indices;
    for (const auto scc : sccs) {
        indices.insert(indexOfScc(scc));
    }
    return indices;
}

}  // namespace souffle::ast::analysis
