/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/***************************************************************************
 *
 * @file IndexSetAnalysis.h
 *
 * Computes indexes for relations in a translation unit
 *
 ***************************************************************************/

#pragma once

#include "RamAnalysis.h"
#include "RamRelation.h"
#include "RamTypes.h"
#include "Util.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define NIL 0
#define INF -1

// define if enable unit tests
#define M_UNIT_TEST

namespace souffle {

/**
 * Computes a maximum matching with Hopcroft-Karp algorithm
 * Source: http://en.wikipedia.org/wiki/Hopcroft%E2%80%93Karp_algorithm#Pseudocode
 */
class MaxMatching {
public:
    typedef std::map<SearchColumns, SearchColumns, std::greater<SearchColumns>> Matchings;
    typedef std::set<SearchColumns, std::greater<SearchColumns>> Nodes;

private:
    typedef std::set<SearchColumns> Edges;
    typedef std::map<SearchColumns, Edges> Graph;
    typedef std::map<SearchColumns, int> Distance;

public:
    /** Solve */
    const Matchings& solve();

    /** Get number of matches */
    int getNumMatchings() const {
        return match.size() / 2;
    }

    /** Add edge */
    void addEdge(SearchColumns u, SearchColumns v);

protected:
    /** Get match */
    SearchColumns getMatch(SearchColumns v);

    /** Get distance */
    int getDistance(int v);

    /** breadth first search */
    bool bfSearch();

    /** depth first search */
    bool dfSearch(SearchColumns u);

private:
    Matchings match;
    Graph graph;
    Distance distance;
};

/**
 * Computes the index set for a relation
 *
 * If the indexes of a relation can cover several searches, the minimal
 * set of indexes is computed by Dilworth's problem. See
 *
 * "Optimal On The Fly Index Selection in Polynomial Time"
 * https://arxiv.org/abs/1709.03685
 *
 */

class IndexSet {
public:
    typedef std::vector<int> LexicographicalOrder;
    typedef std::vector<LexicographicalOrder> OrderCollection;

protected:
    typedef std::set<SearchColumns> Chain;
    typedef std::vector<Chain> ChainOrderMap;
    typedef std::set<SearchColumns> SearchSet;

    SearchSet searches;          // set of search patterns on table
    OrderCollection orders;      // collection of lexicographical orders
    ChainOrderMap chainToOrder;  // maps order index to set of searches covered by chain

    MaxMatching matching;         // matching problem for finding minimal number of orders
    const RamRelation& relation;  // relation

public:
    IndexSet(const RamRelation& rel) : relation(rel) {}

    /** Add new key to an Index Set */
    inline void addSearch(SearchColumns cols) {
        if (cols != 0) {
            searches.insert(cols);
        }
    }

    /** Get relation */
    const RamRelation& getRelation() const {
        return relation;
    }

    /** Get searches **/
    const SearchSet& getSearches() const {
        return searches;
    }

    /** Get index for a search */
    const LexicographicalOrder getLexOrder(SearchColumns cols) const {
        int idx = map(cols);
        return orders[idx];
    }

    /** Get all indexes */
    const OrderCollection getAllOrders() const {
        return orders;
    }

    /** check whether number of bits in k is not equal
        to number of columns in lexicographical order */
    bool isSubset(SearchColumns cols) const {
        int idx = map(cols);
        return card(cols) < orders[idx].size();
    }

    /** map the keys in the key set to lexicographical order */
    void solve();

    /** convert from a representation of A verticies to B verticies */
    static SearchColumns toB(SearchColumns a) {
        SearchColumns msb = 1;
        msb <<= (4 * 8 - 1);
        return (a | msb);
    }

    /** convert from a representation of B verticies to A verticies */
    static SearchColumns toA(SearchColumns b) {
        SearchColumns msb = 1;
        msb <<= (4 * 8 - 1);
        return (b xor msb);
    }

protected:
    /** count the number of bits in key */
    // TODO: replace by intrinsic of GCC
    static size_t card(SearchColumns cols) {
        size_t sz = 0, idx = 1;
        for (size_t i = 0; i < sizeof(SearchColumns) * 8; i++) {
            if (idx & cols) {
                sz++;
            }
            idx *= 2;
        }
        return sz;
    }

    /** maps search columns to an lexicographical order (labeled by a number) */
    int map(SearchColumns cols) const {
        ASSERT(orders.size() == chainToOrder.size() && "Order and Chain Sizes do not match!!");
        int i = 0;
        for (auto it = chainToOrder.begin(); it != chainToOrder.end(); ++it, ++i) {
            if (it->find(cols) != it->end()) {
                ASSERT((size_t)i < orders.size());
                return i;
            }
        }
        abort();
    }

    /** determine if key a is a strict subset of key b*/
    static bool isStrictSubset(SearchColumns a, SearchColumns b) {
        SearchColumns tt = static_cast<SearchColumns>(std::numeric_limits<SearchColumns>::max());
        return (~(a) | (b)) == tt && a != b;
    }

    /** insert an index based on the delta*/
    void insertIndex(std::vector<int>& ids, SearchColumns delta) {
        int pos = 0;
        SearchColumns mask = 0;

        while (mask < delta) {
            mask = SearchColumns(1 << (pos));
            SearchColumns result = (delta) & (mask);
            if (result) {
                ids.push_back(pos);
            }
            pos++;
        }
    }

    /** given an unmapped node from set A we follow it from set B until it cannot be matched from B
        if not mateched from B then umn is a chain*/
    Chain getChain(const SearchColumns umn, const MaxMatching::Matchings& match);

    /** get all chains from the matching */
    const ChainOrderMap getChainsFromMatching(const MaxMatching::Matchings& match, const SearchSet& nodes);

    /** get all nodes which are unmated from A-> B */
    const SearchSet getUnmatchedKeys(const MaxMatching::Matchings& match, const SearchSet& nodes) {
        ASSERT(!nodes.empty());
        SearchSet unmatched;

        // For all nodes n such that n is not in match
        for (SearchSet::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
            if (match.find(*it) == match.end()) {
                unmatched.insert(*it);
            }
        }
        return unmatched;
    }
};

/**
 * Analysis pass computing the index sets of RAM relations
 */
class IndexSetAnalysis : public RamAnalysis {
private:
    typedef std::map<std::string, IndexSet> data_t;
    typedef typename data_t::iterator iterator;
    std::map<std::string, IndexSet> data;

public:
    static constexpr const char* name = "index-analysis";

    /** run analysis */
    void run(const RamTranslationUnit& translationUnit) override;

    /** print analysis */
    void print(std::ostream& os) const override;

    /** get indexes */
    IndexSet& getIndexes(const RamRelation& rel) {
        auto pos = data.find(rel.getName());
        if (pos != data.end()) {
            return pos->second;
        } else {
            auto ret = data.insert(make_pair(rel.getName(), IndexSet(rel)));
            assert(ret.second);
            return ret.first->second;
        }
    }
};

}  // end of namespace souffle
