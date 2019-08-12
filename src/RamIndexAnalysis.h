/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/***************************************************************************
 *
 * @file RamIndexAnalysis.h
 *
 * Computes indexes for relations in a translation unit
 *
 ***************************************************************************/

#pragma once

#include "RamAnalysis.h"
#include "RamOperation.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTypes.h"
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iosfwd>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define RIA_NIL 0
#define RIA_INF -1

// define if enable unit tests
#define M_UNIT_TEST

namespace souffle {

class RamTranslationUnit;

/**
 * @class MaxMatching
 * @Brief Computes a maximum matching with Hopcroft-Karp algorithm
 *
 * This class is a helper class for RamIndexAnalysis.
 *
 * This implements a standard maximum matching algorithm for a bi-partite graph
 * also known as a marriage problem. Given a set of edges in a bi-partite graph
 * select a subset of edges that each node in the bi-partite graph has at most
 * one adjacent edge associated with.
 *
 * The nodes of the bi-partite graph represent index-signatures stemming from
 * RAM operations and RAM existence checks for a relation. A relation between
 * two nodes represent whether an index operation subsumes another operation.
 *
 * Source: http://en.wikipedia.org/wiki/Hopcroft%E2%80%93Karp_algorithm#Pseudocode
 */
class MaxMatching {
public:
    /* The nodes of the bi-partite graph are index signatures of RAM operation */
    using Nodes = std::set<SearchSignature, std::greater<SearchSignature>>;

    /**
     * Matching represent a solution of the matching, i.e., which node in the bi-partite
     * graph maps to another node. If no map exist for a node, there is no adjacent edge
     * exists for that node.
     */
    using Matchings = std::map<SearchSignature, SearchSignature, std::greater<SearchSignature>>;

public:
    /**
     * @Brief solve the maximum matching problem
     * @result returns the matching
     */
    const Matchings& solve();

    /**
     * @Brief get number of matches in the solution
     * @return number of matches
     */
    int getNumMatchings() const {
        return match.size() / 2;
    }

    /**
     * @Brief add an edge to the bi-partite graph
     * @param u search signature
     * @param v subsuming search signature
     */
    void addEdge(SearchSignature u, SearchSignature v);

protected:
    /**
     * @Brief get match for a search signature
     * @param v search signature
     */
    SearchSignature getMatch(SearchSignature v);

    /**
     * @Brief get distance of a node
     */
    int getDistance(SearchSignature v);

    /**
     * @Brief perform a breadth first search in the graph
     */
    bool bfSearch();

    /**
     * @Brief perform a depth first search in the graph
     * @param u search signature
     */
    bool dfSearch(SearchSignature u);

private:
    /**
     * Edges in the bi-partite graph
     */
    using Edges = std::set<SearchSignature>;
    /**
     * Bi-partite graph of instance
     */
    using Graph = std::map<SearchSignature, Edges>;
    /**
     * distance function of nodes
     */
    using Distance = std::map<SearchSignature, int>;

    Matchings match;
    Graph graph;
    Distance distance;
};

/**
 * @class MinIndexSelection
 * @Brief computes the minimal index cover for a relation
 *        in a RAM Program.
 *
 * If the indexes of a relation can cover several searches, the minimal
 * set of indexes is computed by Dilworth's problem. See
 *
 * "Automatic Index Selection for Large-Scale Datalog Computation"
 * http://www.vldb.org/pvldb/vol12/p141-subotic.pdf
 *
 */

class MinIndexSelection {
public:
    using LexOrder = std::vector<int>;
    using OrderCollection = std::vector<LexOrder>;
    using Chain = std::set<SearchSignature>;
    using ChainOrderMap = std::vector<Chain>;
    using SearchSet = std::set<SearchSignature>;

    MinIndexSelection() = default;
    ~MinIndexSelection() = default;

    /** @Brief Add new key to an Index Set */
    inline void addSearch(SearchSignature cols) {
        if (cols != 0) {
            searches.insert(cols);
        }
    }

    /** @Brief Get searches **/
    const SearchSet& getSearches() const {
        return searches;
    }

    /** @Brief Get index for a search */
    const LexOrder getLexOrder(SearchSignature cols) const {
        int idx = map(cols);
        return orders[idx];
    }

    /** @Brief Get index for a search */
    const int getLexOrderNum(SearchSignature cols) const {
        return map(cols);
    }

    /** @Brief Get all indexes */
    const OrderCollection getAllOrders() const {
        return orders;
    }

    /** @Brief Get all chains */
    const ChainOrderMap getAllChains() const {
        return chainToOrder;
    }

    /** @Brief check whether number of bits in k is not equal
        to number of columns in lexicographical order */
    bool isSubset(SearchSignature cols) const {
        int idx = map(cols);
        return card(cols) < orders[idx].size();
    }

    /** @Brief map the keys in the key set to lexicographical order */
    void solve();

    /** @Brief convert from a representation of A vertices to B vertices */
    static SearchSignature toB(SearchSignature a) {
        SearchSignature msb = 1;
        msb <<= (4 * 8 - 1);
        return (a | msb);
    }

    /** @Brief convert from a representation of B vertices to A vertices */
    static SearchSignature toA(SearchSignature b) {
        SearchSignature msb = 1;
        msb <<= (4 * 8 - 1);
        return (b xor msb);
    }

    /** @Brief insert a total order index
     *  @param size of the index
     */
    void insertDefaultTotalIndex(size_t arity) {
        Chain chain = std::set<SearchSignature>();
        SearchSignature fullIndexKey = (1 << arity) - 1;
        chain.insert(fullIndexKey);
        chainToOrder.push_back(std::move(chain));
        LexOrder totalOrder;
        for (size_t i = 0; i < arity; ++i) {
            totalOrder.push_back(i);
        }
        orders.push_back(std::move(totalOrder));
    }

protected:
    SearchSet searches;          // set of search patterns on table
    OrderCollection orders;      // collection of lexicographical orders
    ChainOrderMap chainToOrder;  // maps order index to set of searches covered by chain
    MaxMatching matching;        // matching problem for finding minimal number of orders

    /** @Brief count the number of bits in key */
    static size_t card(SearchSignature cols) {
        size_t sz = 0, idx = 1;
        for (size_t i = 0; i < sizeof(SearchSignature) * 8; i++) {
            if (idx & cols) {
                sz++;
            }
            idx *= 2;
        }
        return sz;
    }

    /** @Brief maps search columns to an lexicographical order (labeled by a number) */
    int map(SearchSignature cols) const {
        assert(orders.size() == chainToOrder.size() && "Order and Chain Sizes do not match!!");
        int i = 0;
        for (auto it = chainToOrder.begin(); it != chainToOrder.end(); ++it, ++i) {
            if (it->find(cols) != it->end()) {
                assert((size_t)i < orders.size());
                return i;
            }
        }
        std::cerr << "Cannot find matching lexicographical order" << std::endl;
        abort();
    }

    /** @Brief determine if key a is a strict subset of key b*/
    static bool isStrictSubset(SearchSignature a, SearchSignature b) {
        auto tt = static_cast<SearchSignature>(std::numeric_limits<SearchSignature>::max());
        return (~(a) | (b)) == tt && a != b;
    }

    /** @Brief insert an index based on the delta */
    void insertIndex(LexOrder& ids, SearchSignature delta) {
        int pos = 0;
        SearchSignature mask = 0;

        while (mask < delta) {
            mask = SearchSignature(1 << (pos));
            SearchSignature result = (delta) & (mask);
            if (result) {
                ids.push_back(pos);
            }
            pos++;
        }
    }

    /** @Brief get a chain from a matching
     *  @param Starting node of a chain
     *  @param Matching
     *  @result A minimal chain
     * given an unmapped node from set A
     * we follow it from set B until it cannot be matched from B
     * if not matched from B then umn is a chain.
     */
    Chain getChain(const SearchSignature umn, const MaxMatching::Matchings& match);

    /** @Brief get all chains from the matching */
    const ChainOrderMap getChainsFromMatching(const MaxMatching::Matchings& match, const SearchSet& nodes);

    /** @Brief get all nodes which are unmatched from A-> B */
    const SearchSet getUnmatchedKeys(const MaxMatching::Matchings& match, const SearchSet& nodes) {
        SearchSet unmatched;

        // For all nodes n such that n is not in match
        for (auto node : nodes) {
            if (match.find(node) == match.end()) {
                unmatched.insert(node);
            }
        }
        return unmatched;
    }
};

/**
 * @class RamIndexAnalyis
 * @Brief Analysis pass computing the index sets of RAM relations
 */
class RamIndexAnalysis : public RamAnalysis {
public:
    static constexpr const char* name = "index-analysis";

    void run(const RamTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    /**
     * @Brief get the minimal index cover for a relation
     * @param relation
     * @result set of indexes of the minimal index cover
     */
    MinIndexSelection& getIndexes(const RamRelation& rel);

    /**
     * @Brief get the minimal index cover for a relation
     * @param relation name
     * @result set of indexes of the minimal index cover
     */
    MinIndexSelection& getIndexes(const std::string& relName);

    /**
     * @Brief Get index signature for an Ram IndexOperation operation
     * @param  Index-relation-search operation
     * @result Index signature of operation
     */
    SearchSignature getSearchSignature(const RamIndexOperation* search) const;

    /**
     * @Brief Get the index signature for an existence check
     * @param Existence check
     * @result index signature of existence check
     */
    SearchSignature getSearchSignature(const RamExistenceCheck* existCheck) const;

    /**
     * @Brief Get the index signature for a provenance existence check
     * @param Provenance-existence check
     * @result index signature of provenance-existence check
     */
    SearchSignature getSearchSignature(const RamProvenanceExistenceCheck* existCheck) const;

    /**
     * @Brief Get the default index signature for a relation (the total-order index)
     * @param RamCreate node
     * @result total full-signature of the relation
     */
    SearchSignature getSearchSignature(const RamRelation* ramRel) const;

    /**
     * @Brief index signature of existence check resembles a total index
     * @param (provenance) existence check
     *
     * isTotalSignature returns true if all elements of a tuple are used for the
     * the existence check.
     */
    bool isTotalSignature(const RamAbstractExistenceCheck* existCheck) const;

private:
    /**
     * minimal index cover for relations, i.e., maps a relation to a set of indexes
     */
    std::map<const RamRelation*, MinIndexSelection> minIndexCover;
};

}  // end of namespace souffle
