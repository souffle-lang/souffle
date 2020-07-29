/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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

#include "ast/analysis/Analysis.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace souffle {

class AstTranslationUnit;
class SCCGraphAnalysis;

/**
 * Analysis pass computing a topologically sorted strongly connected component (SCC) graph.
 */
class TopologicallySortedSCCGraphAnalysis : public AstAnalysis {
public:
    static constexpr const char* name = "topological-scc-graph";

    TopologicallySortedSCCGraphAnalysis() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    const std::vector<size_t>& order() const {
        return sccOrder;
    }

    size_t sccOfIndex(const size_t index) const {
        return sccOrder.at(index);
    }

    size_t indexOfScc(const size_t scc) const {
        auto it = std::find(sccOrder.begin(), sccOrder.end(), scc);
        assert(it != sccOrder.end());
        return (size_t)std::distance(sccOrder.begin(), it);
    }

    std::set<size_t> indexOfScc(const std::set<size_t>& sccs) const {
        std::set<size_t> indices;
        for (const auto scc : sccs) {
            indices.insert(indexOfScc(scc));
        }
        return indices;
    }

    /** Output topologically sorted strongly connected component graph in text format */
    void print(std::ostream& os) const override;

private:
    /** The strongly connected component (SCC) graph. */
    SCCGraphAnalysis* sccGraph = nullptr;

    /** The final topological ordering of the SCCs. */
    std::vector<size_t> sccOrder;

    /** Calculate the topological ordering cost of a permutation of as of yet unordered SCCs
    using the ordered SCCs. Returns -1 if the given vector is not a valid topological ordering. */
    int topologicalOrderingCost(const std::vector<size_t>& permutationOfSCCs) const;

    /** Recursive component for the forwards algorithm computing the topological ordering of the SCCs. */
    void computeTopologicalOrdering(size_t scc, std::vector<bool>& visited);
};

}  // end of namespace souffle
