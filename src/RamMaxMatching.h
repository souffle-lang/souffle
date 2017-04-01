/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MaxMatching.h
 *
 * Defines classes of the Hopcroft-Karp algorithm
 * Source: http://en.wikipedia.org/wiki/Hopcroft%E2%80%93Karp_algorithm#Pseudocode
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "Util.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <set>

#define NIL 0
#define INF -1

// define if enable unit tests
#define M_UNIT_TEST

namespace souffle {

class RamMaxMatching {
public:
    typedef std::map<SearchColumns, SearchColumns, std::greater<SearchColumns>> Matchings;
    typedef std::set<SearchColumns, std::greater<SearchColumns>> Nodes;

private:
    typedef std::set<SearchColumns> Edges;
    typedef std::map<SearchColumns, Edges> Graph;
    typedef std::map<SearchColumns, int> Distance;

private:
    Matchings match;    // if x not in match assume match[x] == 0 else a value. Needs both edges...
    Graph graph;        // Only traversed not modified
    Distance distance;  // if x not in distance assume distance[x] == inf. distance [0] special

public:
    /** calculates max matching */
    const Matchings& calculate() {
        while (bfSearch()) {
            for (Graph::iterator it = graph.begin(); it != graph.end(); ++it) {
                if (getMatch(it->first) == NIL) {
                    dfSearch(it->first);
                }
            }
        }
        return match;
    }

    int getNumMatchings() {
        return match.size() / 2;
    }

    /** add an edge to the bi-partite graph */
    void addEdge(SearchColumns u, SearchColumns v) {
        if (graph.find(u) == graph.end()) {
            Edges vals;
            vals.insert(v);
            graph.insert(make_pair(u, vals));
        } else {
            graph[u].insert(v);
        }
    }

protected:
    /** returns match of v */
    inline SearchColumns getMatch(SearchColumns v) {
        Matchings::iterator it = match.find(v);
        if (it == match.end()) {
            return NIL;
        }
        return it->second;
    }

    /** returns distance of v */
    inline int getDistance(int v) {
        Distance::iterator it = distance.find(v);
        if (it == distance.end()) {
            return INF;
        }
        return it->second;
    }

    /** breadth first search */
    bool bfSearch();

    /** depth first search */
    bool dfSearch(SearchColumns u);
};

}  // end of namespace souffle
