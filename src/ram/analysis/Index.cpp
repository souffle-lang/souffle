/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, 2015 Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Index.cpp
 *
 * Computes indexes for relations in a translation unit
 *
 ***********************************************************************/

#include "ram/analysis/Index.h"
#include "Global.h"
#include "RelationTag.h"
#include "ram/CountUniqueKeys.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/Swap.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Relation.h"
#include "ram/utility/Utils.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/StreamUtil.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <queue>

namespace souffle::ram::analysis {

SearchSignature::SearchSignature(std::size_t arity) : constraints(arity, AttributeConstraint::None) {}

bool SearchSignature::operator==(const SearchSignature& other) const {
    assert(constraints.size() == other.constraints.size());
    return constraints == other.constraints;
}

bool SearchSignature::operator!=(const SearchSignature& other) const {
    return !(*this == other);
}

bool SearchSignature::empty() const {
    return std::all_of(constraints.begin(), constraints.end(),
            [](AttributeConstraint c) { return c == AttributeConstraint::None; });
}

AttributeConstraint& SearchSignature::operator[](std::size_t pos) {
    assert(pos < constraints.size());
    return constraints[pos];
}

const AttributeConstraint& SearchSignature::operator[](std::size_t pos) const {
    assert(pos < constraints.size());
    return constraints[pos];
}

std::size_t SearchSignature::arity() const {
    return constraints.size();
}

// comparison operators
bool SearchSignature::precedes(const SearchSignature& other) const {
    assert(arity() == other.arity());
    // ignore duplicates
    if (*this == other) {
        return false;
    }

    // (1) LHS is a subset of RHS
    for (std::size_t i = 0; i < other.arity(); ++i) {
        if (constraints[i] != AttributeConstraint::None) {
            if (other.constraints[i] == AttributeConstraint::None) {
                return false;
            }
        }
    }

    // (2) If RHS has an inequality then LHS can't have that attribute
    for (std::size_t i = 0; i < other.arity(); ++i) {
        if (other.constraints[i] == AttributeConstraint::Inequal) {
            if (constraints[i] != AttributeConstraint::None) {
                return false;
            }
        }
    }
    return true;
}
SearchSignature SearchSignature::getDelta(const SearchSignature& lhs, const SearchSignature& rhs) {
    assert(lhs.arity() == rhs.arity());
    SearchSignature delta(lhs.arity());
    for (std::size_t i = 0; i < lhs.arity(); ++i) {
        // if rhs is empty then delta is just lhs
        if (rhs[i] == AttributeConstraint::None) {
            delta.constraints[i] = lhs[i];
            // otherwise no delta
        } else {
            delta.constraints[i] = AttributeConstraint::None;
        }
    }
    return delta;
}

SearchSignature SearchSignature::getFullSearchSignature(std::size_t arity) {
    SearchSignature res(arity);
    std::for_each(res.begin(), res.end(), [](auto& constraint) { constraint = AttributeConstraint::Equal; });
    return res;
}

std::ostream& operator<<(std::ostream& out, const SearchSignature& signature) {
    std::size_t len = signature.constraints.size();
    for (std::size_t i = 0; i < len; ++i) {
        switch (signature.constraints[i]) {
            case AttributeConstraint::None: out << 0; break;
            case AttributeConstraint::Equal: out << 1; break;
            case AttributeConstraint::Inequal: out << 2; break;
        }
    }
    return out;
}

void MaxMatching::addEdge(Node u, Node v) {
    assert(u >= 1 && v >= 1 && "Nodes must be greater than or equal to 1");
    if (graph.find(u) == graph.end()) {
        Edges vals;
        vals.insert(v);
        graph.insert(make_pair(u, vals));
    } else {
        graph[u].insert(v);
    }
}

MaxMatching::Node MaxMatching::getMatch(Node v) {
    auto it = match.find(v);
    if (it == match.end()) {
        return NullVertex;
    }
    return it->second;
}

MaxMatching::Distance MaxMatching::getDistance(Node v) {
    auto it = distance.find(v);
    if (it == distance.end()) {
        return InfiniteDistance;
    }
    return it->second;
}

bool MaxMatching::bfSearch() {
    Node u;
    std::queue<Node> bfQueue;
    // Build layers
    for (auto& it : graph) {
        if (getMatch(it.first) == NullVertex) {
            distance[it.first] = 0;
            bfQueue.push(it.first);
        } else {
            distance[it.first] = InfiniteDistance;
        }
    }

    distance[NullVertex] = InfiniteDistance;
    while (!bfQueue.empty()) {
        u = bfQueue.front();
        bfQueue.pop();
        assert(u != NullVertex);
        const Edges& children = graph[u];
        for (auto it : children) {
            Node mv = getMatch(it);
            if (getDistance(mv) == InfiniteDistance) {
                distance[mv] = getDistance(u) + 1;
                if (mv != NullVertex) {
                    bfQueue.push(mv);
                }
            }
        }
    }
    return (getDistance(NullVertex) != InfiniteDistance);
}

bool MaxMatching::dfSearch(Node u) {
    if (u != 0) {
        Edges& children = graph[u];
        for (auto v : children) {
            if (getDistance(getMatch(v)) == getDistance(u) + 1) {
                if (dfSearch(getMatch(v))) {
                    match[u] = v;
                    match[v] = u;
                    return true;
                }
            }
        }

        distance[u] = InfiniteDistance;
        return false;
    }
    return true;
}

const MaxMatching::Matchings& MaxMatching::solve() {
    while (bfSearch()) {
        std::vector<Node> keys(graph.size());
        for (auto& it : graph) {
            keys.push_back(it.first);
        }
        for (auto node : keys) {
            if (getMatch(node) == NullVertex) {
                dfSearch(node);
            }
        }
    }
    return match;
}

IndexCluster MinIndexSelectionStrategy::solve(const SearchSet& searches) const {
    OrderCollection orders;
    SignatureOrderMap indexSelection;

    // If there are no orders then the arity of the relation is zero
    // this is because every non-nullary relation has an existence check
    if (searches.empty()) {
        auto search = SearchSignature::getFullSearchSignature(0);
        LexOrder emptyOrder;
        orders.push_back(emptyOrder);
        indexSelection.insert({search, emptyOrder});
        return IndexCluster(indexSelection, {search}, orders);
    }

    // Map the signatures of each search to a unique node in each bipartition for the matching problem
    SearchBipartiteMap mapping;
    for (auto s : searches) {
        mapping.addSearch(s);
    }

    // Construct the matching poblem
    // For each pair of search sets
    // Draw an edge from LHS to RHS if LHS precedes RHS in the partial order
    MaxMatching matching;
    for (auto left : searches) {
        for (auto right : searches) {
            if (left == right) {
                continue;
            }
            if (left.precedes(right)) {
                matching.addEdge(mapping.getLeftNode(left), mapping.getRightNode(right));
            }
        }
    }

    // Perform the Hopcroft-Karp on the graph and receive matchings (mapped A->B and B->A)
    // Assume: alg.calculate is not called on an empty graph
    assert(!searches.empty());
    const MaxMatching::Matchings& matchings = matching.solve();

    // Extract the chains given the nodes and matchings
    auto chains = getChainsFromMatching(matchings, searches, mapping);

    // Should never get no chains back as we never call calculate on an empty graph
    assert(!chains.empty());
    for (const auto& chain : chains) {
        std::vector<std::size_t> ids;

        SearchSignature initDelta = *(chain.begin());
        insertIndex(ids, initDelta);

        // Build the lex-order
        for (auto iit = chain.begin(); next(iit) != chain.end(); ++iit) {
            SearchSignature delta = SearchSignature::getDelta(*next(iit), *iit);
            insertIndex(ids, delta);
        }

        assert(!ids.empty());
        orders.push_back(ids);
    }

    // Validate the lex-order
    for (auto chain : chains) {
        for (auto search : chain) {
            std::size_t idx = map(search, orders, chains);

            // Rebuild the search from the order
            SearchSignature k(search.arity());
            std::size_t numConstraints = std::count_if(
                    search.begin(), search.end(), [](auto c) { return c != AttributeConstraint::None; });

            // Map the k-th prefix of the order to a search
            for (std::size_t i = 0; i < numConstraints; i++) {
                k[orders[idx][i]] = AttributeConstraint::Equal;
            }

            // Validate that the prefix concides with the original search (ignoring inequalities)
            for (std::size_t i = 0; i < search.arity(); ++i) {
                if (k[i] == AttributeConstraint::None && search[i] != AttributeConstraint::None) {
                    assert("incorrect lexicographical order");
                }
                if (k[i] != AttributeConstraint::None && search[i] == AttributeConstraint::None) {
                    assert("incorrect lexicographical order");
                }
            }
        }
    }

    // Return the index selection
    for (const auto& search : searches) {
        std::size_t orderIndex = map(search, orders, chains);
        indexSelection.insert({search, orders.at(orderIndex)});
    }

    return IndexCluster(indexSelection, searches, orders);
}

Chain MinIndexSelectionStrategy::getChain(const SearchSignature& umn, const MaxMatching::Matchings& match,
        const SearchBipartiteMap& mapping) const {
    const SearchSignature* start = &umn;  // start at an unmatched node
    Chain chain;
    // given an unmapped node from set A we follow it from set B until it cannot be matched from B
    //  if not matched from B then umn is a chain
    //
    // Assume : no circular mappings, i.e. a in A -> b in B -> ........ -> a in A is not allowed.
    // Given this, the loop will terminate
    while (true) {
        auto mit = match.find(mapping.getRightNode(*start));  // we start from B side
        // on each iteration we swap sides when collecting the chain so we use the corresponding index map
        if (std::find(chain.begin(), chain.end(), *start) == chain.end()) {
            chain.push_back(*start);
        }

        if (mit == match.end()) {
            std::reverse(chain.begin(), chain.end());
            return chain;
        }

        const SearchSignature& a = mapping.getSearch(mit->second);
        if (std::find(chain.begin(), chain.end(), a) == chain.end()) {
            chain.push_back(a);
        }
        start = &a;
    }
}

const ChainOrderMap MinIndexSelectionStrategy::getChainsFromMatching(const MaxMatching::Matchings& match,
        const SearchSet& nodes, const SearchBipartiteMap& mapping) const {
    assert(!nodes.empty());
    ChainOrderMap chainToOrder;

    // Get all unmatched nodes from A
    const SearchSet& umKeys = getUnmatchedKeys(match, nodes, mapping);
    // Case: if no unmatched nodes then we have an anti-chain
    if (umKeys.empty()) {
        for (auto node : nodes) {
            Chain a = {node};
            chainToOrder.push_back(a);
            return chainToOrder;
        }
    }

    assert(!umKeys.empty());

    // A worklist of used nodes
    SearchSet usedKeys;

    // Case: nodes < umKeys or if nodes == umKeys then anti chain - this is handled by this loop
    for (auto umKey : umKeys) {
        auto c = getChain(umKey, match, mapping);
        assert(!c.empty());
        chainToOrder.push_back(c);
    }

    assert(!chainToOrder.empty());
    return chainToOrder;
}

void IndexAnalysis::run(const TranslationUnit& translationUnit) {
    relAnalysis = &translationUnit.getAnalysis<RelationAnalysis>();

    // After complete:
    // 1. All relations should have at least one index (for full-order search).
    // 2. Two relations involved in a swap operation will have same set of indices.
    // 3. A 0-arity relation will have only one index where LexOrder is defined as empty. A comparator using
    // an empty order should regard all elements as equal and therefore only allow one arbitrary tuple to be
    // inserted.
    //
    // TODO:
    // 0-arity relation in a provenance program still need to be revisited.
    // visit all nodes to collect searches of each relation

    // visit all nodes to collect searches of each relation
    visit(translationUnit.getProgram(), [&](const Node& node) {
        if (const auto* countUniqueKeys = as<CountUniqueKeys>(node)) {
            relationToSearches[countUniqueKeys->getRelation()].insert(getSearchSignature(countUniqueKeys));
        } else if (const auto* indexSearch = as<IndexOperation>(node)) {
            relationToSearches[indexSearch->getRelation()].insert(getSearchSignature(indexSearch));
        } else if (const auto* exists = as<ExistenceCheck>(node)) {
            relationToSearches[exists->getRelation()].insert(getSearchSignature(exists));
        } else if (const auto* provExists = as<ProvenanceExistenceCheck>(node)) {
            relationToSearches[provExists->getRelation()].insert(getSearchSignature(provExists));
        } else if (const auto* aggExists = as<AggregateExistenceCheck>(node)) {
            relationToSearches[aggExists->getRelation()].insert(getSearchSignature(aggExists));
        } else if (const auto* ramRel = as<Relation>(node)) {
            relationToSearches[ramRel->getName()].insert(getSearchSignature(ramRel));
        }
    });

    // A swap happen between rel A and rel B indicates A should include all indices of B, vice versa.
    visit(translationUnit.getProgram(), [&](const Swap& swap) {
        // Note: this naive approach will not work if there exists chain or cyclic swapping.
        // e.g.  swap(relA, relB) swap(relB, relC) swap(relC, relA)
        // One need to keep merging the search set until a fixed point where no more index is introduced
        // in any of the relation in a complete iteration.
        //
        // Currently RAM does not have such situation.
        const std::string& relA = swap.getFirstRelation();
        const std::string& relB = swap.getSecondRelation();

        const auto searchesA = relationToSearches[relA];
        const auto searchesB = relationToSearches[relB];

        relationToSearches[relA].insert(searchesB.begin(), searchesB.end());
        relationToSearches[relB].insert(searchesA.begin(), searchesA.end());
    });

    // remove all empty searches
    for (auto& relToSearch : relationToSearches) {
        auto& searches = relToSearch.second;
        for (auto it = searches.begin(); it != searches.end();) {
            if (it->empty()) {
                it = searches.erase(it);
            } else {
                ++it;
            }
        }
    }

    // find optimal indexes for relations
    for (auto& relToSearch : relationToSearches) {
        const std::string& relation = relToSearch.first;
        auto& searches = relToSearch.second;
        indexCover.insert({relation, solver->solve(searches)});
    }
}

void IndexAnalysis::print(std::ostream& os) const {
    for (auto& cur : indexCover) {
        const std::string& relName = cur.first;
        const auto& selection = cur.second;

        os << "Relation " << relName << "\n";

        /* print searches */
        os << "\tNumber of Searches: " << selection.getSearches().size() << "\n";
        for (auto& search : selection.getSearches()) {
            os << "\t\t";
            os << search;
            os << "\n";
        }

        /* print indexes */
        os << "\tNumber of Indexes: " << selection.getAllOrders().size() << "\n";
        for (auto& order : selection.getAllOrders()) {
            os << "\t\t";
            os << join(order, "<") << "\n";
            os << "\n";
        }
    }
}

namespace {
// handles equality constraints
template <typename Iter>
SearchSignature searchSignature(std::size_t arity, Iter const& bgn, Iter const& end) {
    SearchSignature keys(arity);

    std::size_t i = 0;
    for (auto cur = bgn; cur != end; ++cur, ++i) {
        if (!isUndefValue(*cur)) {
            keys[i] = AttributeConstraint::Equal;
        }
    }
    return keys;
}

template <typename Seq>
SearchSignature searchSignature(std::size_t arity, Seq const& xs) {
    return searchSignature(arity, xs.begin(), xs.end());
}
}  // namespace

SearchSignature IndexAnalysis::getSearchSignature(const CountUniqueKeys* count) const {
    const Relation* rel = &relAnalysis->lookup(count->getRelation());
    std::size_t arity = rel->getArity();

    // default everything is AttributeConstraint::None
    SearchSignature keys(arity);
    // set join column attributes
    for (std::size_t col : count->getKeyColumns()) {
        keys[col] = AttributeConstraint::Equal;
    }
    return keys;
}

SearchSignature IndexAnalysis::getSearchSignature(const IndexOperation* search) const {
    const Relation* rel = &relAnalysis->lookup(search->getRelation());
    std::size_t arity = rel->getArity();

    auto lower = search->getRangePattern().first;
    auto upper = search->getRangePattern().second;
    SearchSignature keys(arity);
    for (std::size_t i = 0; i < arity; ++i) {
        // if both bounds are undefined
        if (isUndefValue(lower[i]) && isUndefValue(upper[i])) {
            keys[i] = AttributeConstraint::None;
            // if bounds are equal we have an equality
        } else if (*lower[i] == *upper[i]) {
            keys[i] = AttributeConstraint::Equal;
        } else {
            keys[i] = AttributeConstraint::Inequal;
        }
    }
    return keys;
}

SearchSignature IndexAnalysis::getSearchSignature(const AggregateExistenceCheck* aggExistCheck) const {
    const auto values = aggExistCheck->getValues();
    const Relation* rel = &relAnalysis->lookup(aggExistCheck->getRelation());
    auto auxiliaryArity = rel->getAuxiliaryArity();

    SearchSignature keys(values.size());

    // all payload attributes should be equalities
    for (std::size_t i = 0; i < values.size() - auxiliaryArity; i++) {
        if (!isUndefValue(values[i])) {
            keys[i] = AttributeConstraint::Equal;
        }
    }

    // all auxiliary attributes should be free
    for (std::size_t i = values.size() - auxiliaryArity; i < values.size(); i++) {
        keys[i] = AttributeConstraint::None;
    }

    return keys;
}

SearchSignature IndexAnalysis::getSearchSignature(const ProvenanceExistenceCheck* provExistCheck) const {
    const auto values = provExistCheck->getValues();
    const Relation* rel = &relAnalysis->lookup(provExistCheck->getRelation());
    auto auxiliaryArity = rel->getAuxiliaryArity();

    SearchSignature keys(values.size());

    // all payload attributes should be equalities
    for (std::size_t i = 0; i < values.size() - auxiliaryArity; i++) {
        if (!isUndefValue(values[i])) {
            keys[i] = AttributeConstraint::Equal;
        }
    }

    // all auxiliary attributes should be free
    for (std::size_t i = values.size() - auxiliaryArity; i < values.size(); i++) {
        keys[i] = AttributeConstraint::None;
    }

    return keys;
}

SearchSignature IndexAnalysis::getSearchSignature(const ExistenceCheck* existCheck) const {
    const Relation* rel = &relAnalysis->lookup(existCheck->getRelation());
    return searchSignature(rel->getArity(), existCheck->getValues());
}

SearchSignature IndexAnalysis::getSearchSignature(const Relation* ramRel) const {
    return SearchSignature::getFullSearchSignature(ramRel->getArity());
}

bool IndexAnalysis::isTotalSignature(const AbstractExistenceCheck* existCheck) const {
    for (const auto& cur : existCheck->getValues()) {
        if (isUndefValue(cur)) {
            return false;
        }
    }
    return true;
}

}  // namespace souffle::ram::analysis
