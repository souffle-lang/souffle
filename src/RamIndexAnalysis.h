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

#include "Global.h"
#include "RamAnalysis.h"
#include "RamRelation.h"
#include "utility/MiscUtil.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// define if enable unit tests
#define M_UNIT_TEST

namespace souffle {

class RamAbstractExistenceCheck;
class RamExistenceCheck;
class RamIndexOperation;
class RamProvenanceExistenceCheck;
class RamRelation;
class RamTranslationUnit;

enum class AttributeConstraint { None, Equal, Inequal };

/** search signature of a RAM operation; each bit represents an attribute of a relation.
 * A one represents that the attribute has an assigned value; a zero represents that
 * no value exists (i.e. attribute is unbounded) in the search. */
class SearchSignature {
public:
    explicit SearchSignature(size_t arity);
    size_t arity() const;

    // array subscript operator
    AttributeConstraint operator[](std::size_t pos) const;

    // comparison operators
    bool operator<(const SearchSignature& other) const;
    bool operator==(const SearchSignature& other) const;

    bool empty() const;
    static bool isComparable(const SearchSignature& lhs, const SearchSignature& rhs);
    static bool isStrictSubset(const SearchSignature& lhs, const SearchSignature& rhs);
    static bool isWindowQuery(const SearchSignature& s);
    static SearchSignature getDelta(const SearchSignature& lhs, const SearchSignature& rhs);
    static SearchSignature getFullSearchSignature(size_t arity);

    // set a constraint
    SearchSignature& set(size_t pos, AttributeConstraint constraint);
    friend std::ostream& operator<<(std::ostream& out, const SearchSignature& signature);

private:
    std::vector<AttributeConstraint> constraints;
};

std::ostream& operator<<(std::ostream& out, const SearchSignature& signature);

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
    using Node = uint32_t;
    /* The nodes of the bi-partite graph are index signatures of RAM operation */
    using Nodes = std::unordered_set<Node>;
    /* Distance between nodes */
    using Distance = int;
    /**
     * Matching represent a solution of the matching, i.e., which node in the bi-partite
     * graph maps to another node. If no map exist for a node, there is no adjacent edge
     * exists for that node.
     */
    using Matchings = std::unordered_map<Node, Node>;

    /* Node constant representing no match */
    const Node NullVertex = 0;

    /* Infinite distance */
    const Distance InfiniteDistance = -1;

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
    void addEdge(Node u, Node v);

protected:
    /**
     * @Brief get match for a search signature
     * @param v search signature
     */
    Node getMatch(Node v);

    /**
     * @Brief get distance of a node
     */
    Distance getDistance(Node v);

    /**
     * @Brief perform a breadth first search in the graph
     */
    bool bfSearch();

    /**
     * @Brief perform a depth first search in the graph
     * @param u search signature
     */
    bool dfSearch(Node u);

private:
    /**
     * Edges in the bi-partite graph
     */
    using Edges = std::unordered_set<Node>;
    /**
     * Bi-partite graph of instance
     */
    using Graph = std::unordered_map<Node, Edges>;
    /**
     * distance function of nodes
     */
    using DistanceMap = std::map<Node, Distance>;

    Matchings match;
    Graph graph;
    DistanceMap distance;
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
    using AttributeIndex = uint32_t;
    using SignatureMap = std::map<SearchSignature, SearchSignature>;
    using SignatureIndexMap = std::map<SearchSignature, AttributeIndex>;
    using IndexSignatureMap = std::map<AttributeIndex, SearchSignature>;
    using LexOrder = std::vector<AttributeIndex>;
    using OrderCollection = std::vector<LexOrder>;
    using Chain = std::set<SearchSignature>;
    using ChainOrderMap = std::vector<Chain>;
    using SearchSet = std::set<SearchSignature>;
    using AttributeSet = std::unordered_set<AttributeIndex>;

    /** @Brief Add new key to an Index Set */
    inline void addSearch(SearchSignature cols) {
        if (!cols.empty()) {
            for (auto search : searches) {
                if (search == cols) {
                    return;
                }
            }
            searches.insert(cols);
        }
    }

    MinIndexSelection() = default;
    ~MinIndexSelection() = default;

    /** @Brief Get searches **/
    const SearchSet& getSearches() const {
        return searches;
    }

    /** @Brief Get index for a search */
    const LexOrder& getLexOrder(SearchSignature cols) const {
        int idx = map(cols);
        return orders[idx];
    }

    /** @Brief Get index for a search */
    int getLexOrderNum(SearchSignature cols) const {
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

    /** @Brief check whether number of constraints in k is not equal
        to number of columns in lexicographical order */
    bool isSubset(SearchSignature cols) const {
        int idx = map(cols);
        return card(cols) < orders[idx].size();
    }

    /** @Brief map the keys in the key set to lexicographical order */
    void solve();

    /** @Brief insert a total order index
     *  @param size of the index
     */
    void insertDefaultTotalIndex(size_t arity) {
        Chain chain = std::set<SearchSignature>();
        SearchSignature fullIndexKey = SearchSignature::getFullSearchSignature(arity);
        chain.insert(fullIndexKey);
        chainToOrder.push_back(std::move(chain));
        LexOrder totalOrder;
        for (size_t i = 0; i < arity; ++i) {
            totalOrder.push_back(i);
        }
        orders.push_back(std::move(totalOrder));
    }
    /** Return the attribute position for each indexed operation that should be discharged.
     */
    AttributeSet getAttributesToDischarge(const SearchSignature& s, const RamRelation& rel);

protected:
    SignatureIndexMap signatureToIndexA;  // mapping of a SearchSignature on A to its unique index
    SignatureIndexMap signatureToIndexB;  // mapping of a SearchSignature on B to its unique index
    IndexSignatureMap indexToSignature;   // mapping of a unique index to its SearchSignature
    SearchSet searches;                   // set of search patterns on table
    OrderCollection orders;               // collection of lexicographical orders
    ChainOrderMap chainToOrder;           // maps order index to set of searches covered by chain
    MaxMatching matching;                 // matching problem for finding minimal number of orders

    /** @Brief count the number of constraints in key */
    static size_t card(SearchSignature cols) {
        size_t sz = 0;
        for (size_t i = 0; i < cols.arity(); i++) {
            if (cols[i] != AttributeConstraint::None) {
                sz++;
            }
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

        fatal("cannot find matching lexicographical order");
    }

    /** @Brief insert an index based on the delta */
    void insertIndex(LexOrder& ids, SearchSignature delta) {
        LexOrder backlog;  // add inequalities at the end
        for (size_t pos = 0; pos < delta.arity(); pos++) {
            if (delta[pos] == AttributeConstraint::Equal) {
                ids.push_back(pos);
            } else if (delta[pos] == AttributeConstraint::Inequal) {
                backlog.push_back(pos);
            }
        }
        ids.insert(ids.end(), backlog.begin(), backlog.end());
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

    /** @Brief merge chains to produce a minimal chain cover */
    const ChainOrderMap mergeChains(ChainOrderMap& chains);

    /** @Brief get all nodes which are unmatched from A-> B */
    const SearchSet getUnmatchedKeys(const MaxMatching::Matchings& match, const SearchSet& nodes) {
        SearchSet unmatched;

        // For all nodes n such that n is not in match
        for (auto node : nodes) {
            if (match.find(signatureToIndexA[node]) == match.end()) {
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
    RamIndexAnalysis(const char* id) : RamAnalysis(id) {}

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
     * @param ramRel RAM-relation
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
