/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TopologicallySortedSCCGraph.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast {

class TranslationUnit;

namespace analysis {

class SCCGraphAnalysis;

/**
 * Analysis pass computing a topologically sorted strongly connected component (SCC) graph.
 */
class TopologicallySortedSCCGraphAnalysis : public Analysis {
public:
    static constexpr const char* name = "topological-scc-graph";

    explicit TopologicallySortedSCCGraphAnalysis();

    void run(const TranslationUnit& translationUnit) override;

    const std::vector<std::size_t>& order() const;

    std::size_t sccOfIndex(const std::size_t index) const;

    std::size_t indexOfScc(const std::size_t scc) const;

    std::set<std::size_t> indexOfScc(const std::set<std::size_t>& sccs) const;

    /** Output topologically sorted strongly connected component graph in text format */
    void print(std::ostream& os) const override;

private:
    /** The strongly connected component (SCC) graph. */
    SCCGraphAnalysis* sccGraph = nullptr;

    /** The final topological ordering of the SCCs. */
    std::vector<std::size_t> sccOrder;

    /** The mapping from an SCC index to its maximum distance from roots */
    std::map<std::size_t, int> sccDistance;

    /** Calculate the topological ordering cost of a permutation of as of yet unordered SCCs
    using the ordered SCCs. Returns -1 if the given vector is not a valid topological ordering. */
    int topologicalOrderingCost(const std::vector<std::size_t>& permutationOfSCCs) const;

    /** Recursive component for the forwards algorithm computing the topological ordering of the SCCs. */
    void computeTopologicalOrdering(std::size_t scc, std::vector<int>& visited);
};

}  // namespace analysis
}  // namespace souffle::ast
