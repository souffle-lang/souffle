/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IndexAnalysis.cpp
 *
 * Computes indexes for relations in a translation unit
 *
 ***********************************************************************/

#include "IndexSetAnalysis.h"
#include "RamCondition.h"
#include "RamOperation.h"
#include "RamTranslationUnit.h"
#include "RamValue.h"
#include "RamVisitor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <regex>
#include <utility>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace souffle {

/**
 * Class MaxMatching
 */

/** Add edge */
void MaxMatching::addEdge(SearchColumns u, SearchColumns v) {
    if (graph.find(u) == graph.end()) {
        Edges vals;
        vals.insert(v);
        graph.insert(make_pair(u, vals));
    } else {
        graph[u].insert(v);
    }
}

/** Get match */
SearchColumns MaxMatching::getMatch(SearchColumns v) {
    Matchings::iterator it = match.find(v);
    if (it == match.end()) {
        return NIL;
    }
    return it->second;
}

/** Get Distance */
int MaxMatching::getDistance(int v) {
    Distance::iterator it = distance.find(v);
    if (it == distance.end()) {
        return INF;
    }
    return it->second;
}

/** Breadth first search */
bool MaxMatching::bfSearch() {
    SearchColumns u;
    std::queue<SearchColumns> bfQueue;
    // Build layers
    for (Graph::iterator it = graph.begin(); it != graph.end(); ++it) {
        if (getMatch(it->first) == NIL) {
            distance[it->first] = 0;
            bfQueue.push(it->first);
        } else {
            distance[it->first] = INF;
        }
    }
    distance[NIL] = INF;
    while (!bfQueue.empty()) {
        u = bfQueue.front();
        bfQueue.pop();
        ASSERT(u != NIL);
        const Edges& children = graph[u];
        for (Edges::iterator it = children.begin(); it != children.end(); ++it) {
            SearchColumns mv = getMatch(*it);
            if (getDistance(mv) == INF) {
                distance[mv] = getDistance(u) + 1;
                if (mv != NIL) {
                    bfQueue.push(mv);
                }
            }
        }
    }
    return (getDistance(0) != INF);
}

/** Depth first search */
bool MaxMatching::dfSearch(SearchColumns u) {
    if (u != 0) {
        Edges& children = graph[u];
        for (Edges::iterator it = children.begin(); it != children.end(); ++it) {
            SearchColumns v = *it;
            if (getDistance(getMatch(v)) == getDistance(u) + 1) {
                if (dfSearch(getMatch(v))) {
                    match[u] = v;
                    match[v] = u;
                    return true;
                }
            }
        }

        distance[u] = INF;
        return false;
    }
    return true;
}

/** Calculate max-matching */
const MaxMatching::Matchings& MaxMatching::solve() {
    while (bfSearch()) {
        for (Graph::iterator it = graph.begin(); it != graph.end(); ++it) {
            if (getMatch(it->first) == NIL) {
                dfSearch(it->first);
            }
        }
    }
    return match;
}

/*
 * Class IndexSet
 */

/** map the keys in the key set to lexicographical order */
void IndexSet::solve() {
    if (searches.empty()) {
        return;
    }

    // check whether one of the naive indexers should be used
    // two conditions: either set by environment or relation is a hash map
    static const char ENV_NAIVE_INDEX[] = "SOUFFLE_USE_NAIVE_INDEX";
    if (!relation.isCoverable() || std::getenv(ENV_NAIVE_INDEX)) {
        static bool first = true;

        // print a warning - only the first time
        if (relation.isCoverable() && first) {
            std::cout << "WARNING: auto index selection disabled, naive indexes are utilized!!\n";
            first = false;
        }

        // every search pattern gets its naive index
        for (SearchColumns cur : searches) {
            // obtain order
            LexicographicalOrder order;
            SearchColumns mask = cur;
            for (int i = 0; mask != 0; i++) {
                if (!(1 << i & mask)) {
                    continue;
                }
                order.push_back(i);
                // clear bit
                mask &= ~(1 << i);
            }

            // add new order
            orders.push_back(order);

            // register pseudo chain
            chainToOrder.push_back(Chain());
            chainToOrder.back().insert(cur);
        }

        //		std::cout << "Orders: " << orders << "\n";
        //		std::cout << "Chains: " << chainToOrder << "\n";

        return;
    }

    // Construct the matching poblem
    for (SearchSet::const_iterator it = searches.begin(); it != searches.end(); ++it) {
        // For this node check if other nodes are strict subsets
        for (SearchSet::const_iterator itt = searches.begin(); itt != searches.end(); ++itt) {
            if (isStrictSubset(*it, *itt)) {
                matching.addEdge(*it, toB(*itt));
            }
        }
    }

    // Perform the Hopcroft-Karp on the graph and receive matchings (mapped A->B and B->A)
    // Assume: alg.calculate is not called on an empty graph
    ASSERT(!searches.empty());
    const MaxMatching::Matchings& matchings = matching.solve();

    // Extract the chains given the nodes and matchings
    const ChainOrderMap chains = getChainsFromMatching(matchings, searches);

    // Should never get no chains back as we never call calculate on an empty graph
    ASSERT(!chains.empty());

    for (ChainOrderMap::const_iterator it = chains.begin(); it != chains.end(); ++it) {
        std::vector<int> ids;
        SearchColumns initDelta = *(it->begin());
        insertIndex(ids, initDelta);

        for (Chain::iterator iit = it->begin(); next(iit) != it->end(); ++iit) {
            SearchColumns delta = *(next(iit)) - *iit;
            insertIndex(ids, delta);
        }

        ASSERT(!ids.empty());

        orders.push_back(ids);
    }

    // Construct the matching poblem
    for (SearchSet::const_iterator it = searches.begin(); it != searches.end(); ++it) {
        int idx = map(*it);
        size_t l = card(*it);
        SearchColumns k = 0;
        for (size_t i = 0; i < l; i++) {
            k = k + (1 << (orders[idx][i]));
        }
        ASSERT(k == *it && "incorrect lexicographical order");
    }
}

/** given an unmapped node from set A we follow it from set B until it cannot be matched from B
  if not mateched from B then umn is a chain*/
IndexSet::Chain IndexSet::getChain(const SearchColumns umn, const MaxMatching::Matchings& match) {
    SearchColumns start = umn;  // start at an unmateched node
    Chain chain;
    // Assume : no circular mappings, i.e. a in A -> b in B -> ........ -> a in A is not allowed.
    // Given this, the loop will terminate
    while (true) {
        MaxMatching::Matchings::const_iterator mit = match.find(toB(start));  // we start from B side
        chain.insert(start);

        if (mit == match.end()) {
            return chain;
        }

        SearchColumns a = mit->second;
        chain.insert(a);
        start = a;
    }
}

/** get all chains from the matching */
const IndexSet::ChainOrderMap IndexSet::getChainsFromMatching(
        const MaxMatching::Matchings& match, const SearchSet& nodes) {
    ASSERT(!nodes.empty());

    // Get all unmatched nodes from A
    const SearchSet& umKeys = getUnmatchedKeys(match, nodes);

    // Case: if no unmatched nodes then we have an anti-chain
    if (umKeys.empty()) {
        for (SearchSet::const_iterator nit = nodes.begin(); nit != nodes.end(); ++nit) {
            std::set<SearchColumns> a;
            a.insert(*nit);
            chainToOrder.push_back(a);
            return chainToOrder;
        }
    }

    ASSERT(!umKeys.empty());

    // A worklist of used nodes
    SearchSet usedKeys;

    // Case: nodes < umKeys or if nodes == umKeys then anti chain - this is handled by this loop
    for (SearchSet::iterator it = umKeys.begin(); it != umKeys.end(); ++it) {
        Chain c = getChain(*it, match);
        ASSERT(!c.empty());
        chainToOrder.push_back(c);
    }

    ASSERT(!chainToOrder.empty());

    return chainToOrder;
}

/** Compute indexes */
void IndexSetAnalysis::run(const RamTranslationUnit& translationUnit) {
    // visit all nodes to collect searches of each relation
    visitDepthFirst(translationUnit.getP(), [&](const RamNode& node) {
        if (const RamScan* scan = dynamic_cast<const RamScan*>(&node)) {
            IndexSet& indexes = getIndexes(scan->getRelation());
            indexes.addSearch(scan->getRangeQueryColumns());
        } else if (const RamAggregate* agg = dynamic_cast<const RamAggregate*>(&node)) {
            IndexSet& indexes = getIndexes(agg->getRelation());
            indexes.addSearch(agg->getRangeQueryColumns());
        } else if (const RamNotExists* ne = dynamic_cast<const RamNotExists*>(&node)) {
            IndexSet& indexes = getIndexes(ne->getRelation());
            indexes.addSearch(ne->getKey());
        }
    });

    // find optimal indexes for relations
    for (auto& cur : data) {
        IndexSet& indexes = cur.second;
        indexes.solve();
    }
}

/** Print indexes */
void IndexSetAnalysis::print(std::ostream& os) const {
    os << "------ Auto-Index-Generation Report -------\n";
    for (auto& cur : data) {
        const std::string& relName = cur.first;
        const IndexSet& indexes = cur.second;
        const RamRelation& rel = indexes.getRelation();

        /* Print searches */
        os << "Relation " << relName << "\n";
        os << "\tNumber of Search Patterns: " << indexes.getSearches().size() << "\n";

        /* print searches */
        for (auto& cols : indexes.getSearches()) {
            os << "\t\t";
            for (uint32_t i = 0; i < rel.getArity(); i++) {
                if ((1UL << i) & cols) {
                    os << rel.getArg(i) << " ";
                }
            }
            os << "\n";
        }

        os << "\tNumber of Indexes: " << indexes.getAllOrders().size() << "\n";
        for (auto& order : indexes.getAllOrders()) {
            os << "\t\t";
            for (auto& i : order) {
                os << rel.getArg(i) << " ";
            }
            os << "\n";
        }
    }
    os << "------ End of Auto-Index-Generation Report -------\n";
}

}  // end of namespace souffle
