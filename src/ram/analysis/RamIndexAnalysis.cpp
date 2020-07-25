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

#include "ram/analysis/RamIndexAnalysis.h"
#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamProgram.h"
#include "ram/RamRelation.h"
#include "ram/RamStatement.h"
#include "ram/RamTranslationUnit.h"
#include "ram/RamUtils.h"
#include "ram/RamVisitor.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <queue>

namespace souffle {

SearchSignature::SearchSignature(size_t arity) : constraints(arity, AttributeConstraint::None) {}

size_t SearchSignature::arity() const {
    return constraints.size();
}

// convenient operator overload
AttributeConstraint& SearchSignature::operator[](std::size_t pos) {
    assert(pos < constraints.size());
    return constraints[pos];
}

const AttributeConstraint& SearchSignature::operator[](std::size_t pos) const {
    assert(pos < constraints.size());
    return constraints[pos];
}

// comparison operators
bool SearchSignature::operator<(const SearchSignature& other) const {
    assert(constraints.size() == other.constraints.size());
    assert(isComparable(*this, other));  // be ordered to place them in a container together
    if (isStrictSubset(*this, other)) {
        return true;
    } else {
        return false;
    }
}

bool SearchSignature::operator==(const SearchSignature& other) const {
    assert(constraints.size() == other.constraints.size());
    return constraints == other.constraints;
}

bool SearchSignature::operator!=(const SearchSignature& other) const {
    return !(*this == other);
}

bool SearchSignature::empty() const {
    size_t len = constraints.size();
    for (size_t i = 0; i < len; ++i) {
        if (constraints[i] != AttributeConstraint::None) {
            return false;
        }
    }
    return true;
}

bool SearchSignature::containsInequality() const {
    for (size_t i = 0; i < constraints.size(); ++i) {
        if (constraints[i] == AttributeConstraint::Inequal) {
            return true;
        }
    }
    return false;
}

// Note: We have 0 < 1 and 0 < 2 but we cannot say that 1 < 2.
// The reason for this is to prevent search chains such as 100->101->201 which have no valid lex-order
bool SearchSignature::isComparable(const SearchSignature& lhs, const SearchSignature& rhs) {
    assert(lhs.arity() == rhs.arity());
    size_t len = lhs.arity();
    for (size_t i = 0; i < len; ++i) {
        // cannot compare equality and inequality
        if (lhs[i] != rhs[i] && lhs[i] != AttributeConstraint::None && rhs[i] != AttributeConstraint::None) {
            return false;
        }
    }
    // comparable if one is a strict subset of the other
    return (isStrictSubset(lhs, rhs) && !lhs.containsInequality()) ||
           (isStrictSubset(rhs, lhs) && !rhs.containsInequality());
}

bool SearchSignature::isSubset(const SearchSignature& lhs, const SearchSignature& rhs) {
    assert(lhs.arity() == rhs.arity());
    size_t len = lhs.arity();
    for (size_t i = 0; i < len; ++i) {
        if (lhs[i] == rhs[i]) {
            continue;
        }
        if (lhs[i] < rhs[i]) {
            continue;
        }
        if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return true;
}

bool SearchSignature::isStrictSubset(const SearchSignature& lhs, const SearchSignature& rhs) {
    return isSubset(lhs, rhs) && lhs != rhs;
}

SearchSignature SearchSignature::getDelta(const SearchSignature& lhs, const SearchSignature& rhs) {
    assert(lhs.arity() == rhs.arity());
    SearchSignature delta(lhs.arity());
    for (size_t i = 0; i < lhs.arity(); ++i) {
        // if constraints are the same then delta is nothing
        if (lhs.constraints[i] == rhs.constraints[i]) {
            delta.constraints[i] = AttributeConstraint::None;
        } else {
            delta.constraints[i] = lhs.constraints[i];
        }
    }
    return delta;
}

SearchSignature SearchSignature::getFullSearchSignature(size_t arity) {
    SearchSignature res(arity);
    for (size_t i = 0; i < arity; ++i) {
        res.constraints[i] = AttributeConstraint::Equal;
    }
    return res;
}

SearchSignature SearchSignature::getDischarged(const SearchSignature& signature) {
    SearchSignature res = signature;  // copy original
    for (size_t i = 0; i < res.arity(); ++i) {
        if (res[i] == AttributeConstraint::Inequal) {
            res[i] = AttributeConstraint::None;
        }
    }
    return res;
}

std::ostream& operator<<(std::ostream& out, const SearchSignature& signature) {
    size_t len = signature.constraints.size();
    for (size_t i = 0; i < len; ++i) {
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

void MinIndexSelection::solve() {
    // map the keys in the key set to lexicographical order
    if (searches.empty()) {
        return;
    }

    // map the signatures of each search to a unique index for the matching problem
    AttributeIndex currentIndex = 1;
    for (SearchSignature s : searches) {
        if (s.empty()) {
            continue;
        }
        // map the signature to its unique index in each set
        signatureToIndexA.insert({s, currentIndex});
        signatureToIndexB.insert({s, currentIndex + 1});
        // map each index back to the search signature
        indexToSignature.insert({currentIndex, s});
        indexToSignature.insert({currentIndex + 1, s});
        currentIndex += 2;
    }

    // Construct the matching poblem
    for (auto search : searches) {
        // For this node check if other nodes are strict subsets
        for (auto itt : searches) {
            if (SearchSignature::isComparable(search, itt)) {
                if (SearchSignature::isStrictSubset(search, itt)) {
                    // we don't add edges from signatures with inequalities
                    // Example: 210->211 will discharge the first inequality
                    if (!search.containsInequality()) {
                        matching.addEdge(signatureToIndexA[search], signatureToIndexB[itt]);
                    }
                }
            }
        }
    }

    // Perform the Hopcroft-Karp on the graph and receive matchings (mapped A->B and B->A)
    // Assume: alg.calculate is not called on an empty graph
    assert(!searches.empty());
    const MaxMatching::Matchings& matchings = matching.solve();

    // Extract the chains given the nodes and matchings
    ChainOrderMap chains = getChainsFromMatching(matchings, searches);
    // Should never get no chains back as we never call calculate on an empty graph
    assert(!chains.empty());
    for (const auto& chain : chains) {
        std::vector<uint32_t> ids;

        SearchSignature initDelta = *(chain.begin());
        insertIndex(ids, initDelta);

        // build the lex-order
        for (auto iit = chain.begin(); next(iit) != chain.end(); ++iit) {
            bool end = (next(next(iit)) == chain.end());
            // discharge everything but end of chain
            SearchSignature delta =
                    end ? SearchSignature::getDelta(*next(iit), SearchSignature::getDischarged(*iit))
                        : SearchSignature::getDelta(SearchSignature::getDischarged(*next(iit)),
                                  SearchSignature::getDischarged(*iit));

            insertIndex(ids, delta);
        }

        // prune repeated deltas from right to left
        std::reverse(ids.begin(), ids.end());
        std::unordered_set<uint32_t> seen;
        auto newEnd = std::remove_if(ids.begin(), ids.end(), [&seen](uint32_t value) {
            if (seen.find(value) != std::end(seen)) {
                return true;
            } else {
                seen.insert(value);
                return false;
            }
        });

        ids.erase(newEnd, ids.end());
        std::reverse(ids.begin(), ids.end());

        assert(!ids.empty());

        orders.push_back(ids);
    }

    // Construct the matching poblem
    for (auto search : searches) {
        int idx = map(search);
        size_t l = card(search);

        SearchSignature k(search.arity());
        for (size_t i = 0; i < l; i++) {
            k[orders[idx][i]] = AttributeConstraint::Equal;
        }
        for (size_t i = 0; i < search.arity(); ++i) {
            if (k[i] == AttributeConstraint::None && search[i] != AttributeConstraint::None) {
                assert("incorrect lexicographical order");
            }
            if (k[i] != AttributeConstraint::None && search[i] == AttributeConstraint::None) {
                assert("incorrect lexicographical order");
            }
        }
    }
}

MinIndexSelection::Chain MinIndexSelection::getChain(
        const SearchSignature umn, const MaxMatching::Matchings& match) {
    SearchSignature start = umn;  // start at an unmatched node
    Chain chain;
    // given an unmapped node from set A we follow it from set B until it cannot be matched from B
    //  if not mateched from B then umn is a chain
    //
    // Assume : no circular mappings, i.e. a in A -> b in B -> ........ -> a in A is not allowed.
    // Given this, the loop will terminate
    while (true) {
        auto mit = match.find(signatureToIndexB[start]);  // we start from B side
        // on each iteration we swap sides when collecting the chain so we use the corresponding index map
        if (std::find(chain.begin(), chain.end(), start) == chain.end()) {
            chain.push_back(start);
        }

        if (mit == match.end()) {
            std::reverse(chain.begin(), chain.end());
            return chain;
        }

        SearchSignature a = indexToSignature.at(mit->second);
        if (std::find(chain.begin(), chain.end(), a) == chain.end()) {
            chain.push_back(a);
        }
        start = a;
    }
}

const MinIndexSelection::ChainOrderMap MinIndexSelection::getChainsFromMatching(
        const MaxMatching::Matchings& match, const SearchSet& nodes) {
    assert(!nodes.empty());

    // Get all unmatched nodes from A
    const SearchSet& umKeys = getUnmatchedKeys(match, nodes);
    // Case: if no unmatched nodes then we have an anti-chain
    if (umKeys.empty()) {
        for (auto node : nodes) {
            Chain a;
            a.push_back(node);
            chainToOrder.push_back(a);
            return mergeChains(chainToOrder);
        }
    }

    assert(!umKeys.empty());

    // A worklist of used nodes
    SearchSet usedKeys;

    // Case: nodes < umKeys or if nodes == umKeys then anti chain - this is handled by this loop
    for (auto umKey : umKeys) {
        Chain c = getChain(umKey, match);
        assert(!c.empty());
        chainToOrder.push_back(c);
    }

    assert(!chainToOrder.empty());

    return mergeChains(chainToOrder);
}

const MinIndexSelection::ChainOrderMap MinIndexSelection::mergeChains(
        MinIndexSelection::ChainOrderMap& chains) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto lhs_it = chains.begin(); !changed && lhs_it != chains.end(); ++lhs_it) {
            const auto lhs = *lhs_it;
            for (auto rhs_it = std::next(lhs_it); !changed && rhs_it != chains.end(); ++rhs_it) {
                const auto rhs = *rhs_it;

                // merge the two chains
                Chain mergedChain;

                // apply merge algorithm ensuring that both elements are always comparable
                bool successfulMerge = true;
                auto left = lhs.begin();
                auto right = rhs.begin();

                while (left != lhs.end() && right != rhs.end()) {
                    if (SearchSignature::isComparable(*left, *right)) {
                        // if left element is smaller, insert it and iterate to next in left chain
                        if (*left < *right) {
                            mergedChain.push_back(*left);
                            ++left;
                            continue;
                        }
                        // if right element is smaller, insert it and iterate to next in right chain
                        if (*right < *left) {
                            mergedChain.push_back(*right);
                            ++right;
                            continue;
                        }
                    } else {
                        // if they aren't comparable when ignoring the 1->2 restriction we cannot merge since
                        // we have an anti-chain
                        if (!SearchSignature::isSubset(*left, *right) &&
                                !SearchSignature::isSubset(*right, *left)) {
                            successfulMerge = false;
                            break;
                        }

                        // only merge in the circumstance where the delta between left and right contains 1->2
                        // edges
                        auto lower = SearchSignature::isSubset(*left, *right) ? *left : *right;
                        auto upper = SearchSignature::isSubset(*left, *right) ? *right : *left;

                        // cannot merge if lower has inequality since it would be discharged
                        if (lower.containsInequality()) {
                            successfulMerge = false;
                            break;
                        }

                        auto delta = SearchSignature::getDelta(upper, lower);
                        auto prevDelta = mergedChain.empty()
                                                 ? lower
                                                 : SearchSignature::getDelta(lower, mergedChain.back());

                        bool onlyInequalities = true;
                        for (size_t i = 0; i < delta.arity(); ++i) {
                            if (delta[i] == AttributeConstraint::Equal) {
                                onlyInequalities = false;
                                break;
                            }
                        }

                        // If equalities are in the delta set we cannot merge chains without discharging the
                        // inequality Example: 110->211 w/ delta = 201 cannot produce a valid lex-order
                        if (!onlyInequalities) {
                            successfulMerge = false;
                            break;
                        }

                        bool mergable = false;

                        // if we have a 0->2 delta then we can merge
                        // Example: 110->122 we can have the lex-order 0<1<2
                        for (size_t i = 0; i < delta.arity(); ++i) {
                            if (lower[i] == AttributeConstraint::None &&
                                    upper[i] == AttributeConstraint::Inequal) {
                                mergable = true;
                            }
                        }

                        // otherwise if we have one of our inequalities in the prev delta then we can merge
                        // Example: 1000->1011->1021 w/ prevDelta = 0011 delta = 0020
                        for (size_t i = 0; i < delta.arity(); ++i) {
                            if (delta[i] == AttributeConstraint::Inequal) {
                                for (size_t j = 0; j < prevDelta.arity(); ++j) {
                                    if (prevDelta[j] == AttributeConstraint::Equal) {
                                        mergable = true;
                                    }
                                }
                            }
                        }

                        if (!mergable) {
                            successfulMerge = false;
                            break;
                        }

                        // otherwise we merge chains!
                        if (SearchSignature::isSubset(*left, *right)) {
                            mergedChain.push_back(*left);
                            ++left;
                            continue;
                        }

                        if (SearchSignature::isSubset(*right, *left)) {
                            mergedChain.push_back(*right);
                            ++right;
                            continue;
                        }
                    }
                }

                // failed to merge so find another pair of chains
                if (!successfulMerge) {
                    continue;
                }

                // if left chain is exhausted then merge the rest of right chain
                if (left == lhs.end()) {
                    while (right != rhs.end()) {
                        mergedChain.push_back(*right);
                        ++right;
                    }
                }
                // if right chain is exhuasted then merge the rest of left chain
                if (right == rhs.end()) {
                    while (left != lhs.end()) {
                        mergedChain.push_back(*left);
                        ++left;
                    }
                }

                changed = true;

                // remove previous 2 chains
                chains.erase(lhs_it);
                chains.erase(rhs_it);

                // insert merge chain
                chains.push_back(mergedChain);
            }
        }
    }

    return dischargeToMergeChains(chains);
}

const MinIndexSelection::ChainOrderMap MinIndexSelection::dischargeToMergeChains(
        MinIndexSelection::ChainOrderMap& chains) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto lhs_it = chains.begin(); !changed && lhs_it != chains.end(); ++lhs_it) {
            const auto lhs = *lhs_it;
            for (auto rhs_it = std::next(lhs_it); !changed && rhs_it != chains.end(); ++rhs_it) {
                const auto rhs = *rhs_it;

                // merge the two chains
                Chain mergedChain;

                // apply merge algorithm ensuring that both elements are always comparable
                bool successfulMerge = true;
                auto left = lhs.begin();
                auto right = rhs.begin();

                while (left != lhs.end() && right != rhs.end()) {
                    auto leftDischarged = SearchSignature::getDischarged(*left);
                    auto rightDischarged = SearchSignature::getDischarged(*right);

                    if (SearchSignature::isComparable(*left, *right)) {
                        // if left element is smaller, insert it and iterate to next in left chain
                        if (*left < *right) {
                            mergedChain.push_back(*left);
                            ++left;
                            continue;
                        }
                        // if right element is smaller, insert it and iterate to next in right chain
                        if (*right < *left) {
                            mergedChain.push_back(*right);
                            ++right;
                            continue;
                        }
                        // if after discharging one is smaller than the other then we can merge
                    } else if (SearchSignature::isComparable(leftDischarged, *right) &&
                               SearchSignature::isSubset(leftDischarged, *right)) {
                        // if left element is smaller, insert it and iterate to next in left chain
                        if (SearchSignature::isSubset(leftDischarged, *right)) {
                            mergedChain.push_back(*left);
                            ++left;
                            continue;
                        }
                    } else if (SearchSignature::isComparable(rightDischarged, *left) &&
                               SearchSignature::isSubset(rightDischarged, *left)) {
                        // if right element is smaller, insert it and iterate to next in right chain
                        if (SearchSignature::isSubset(rightDischarged, *left)) {
                            mergedChain.push_back(*right);
                            ++right;
                            continue;
                        }
                        // from this point onwards they aren't comparable even after discharging
                    } else {
                        // if they aren't comparable when ignoring the 1->2 restriction we cannot merge since
                        // we have an anti-chain

                        if (!SearchSignature::isSubset(leftDischarged, *right) &&
                                !SearchSignature::isSubset(rightDischarged, *left)) {
                            successfulMerge = false;
                            break;
                        }

                        // only merge in the circumstance where after discharging the delta between left and
                        // right contains 1->2
                        auto lower =
                                SearchSignature::isSubset(leftDischarged, *right) ? leftDischarged : *right;
                        auto upper =
                                SearchSignature::isSubset(rightDischarged, *left) ? rightDischarged : *left;

                        // cannot merge if lower has inequality since it would be discharged
                        if (lower.containsInequality()) {
                            successfulMerge = false;
                            break;
                        }

                        auto delta = SearchSignature::getDelta(upper, lower);
                        auto prevDelta = mergedChain.empty()
                                                 ? lower
                                                 : SearchSignature::getDelta(lower, mergedChain.back());

                        bool onlyInequalities = true;
                        for (size_t i = 0; i < delta.arity(); ++i) {
                            if (delta[i] == AttributeConstraint::Equal) {
                                onlyInequalities = false;
                                break;
                            }
                        }

                        // If equalities are in the delta set we cannot merge chains without discharging the
                        // inequality Example: 110->211 w/ delta = 201 cannot produce a valid lex-order
                        if (!onlyInequalities) {
                            successfulMerge = false;
                            break;
                        }

                        bool mergable = false;

                        // if we have a 0->2 delta then we can merge
                        // Example: 110->122 we can have the lex-order 0<1<2
                        for (size_t i = 0; i < delta.arity(); ++i) {
                            if (lower[i] == AttributeConstraint::None &&
                                    upper[i] == AttributeConstraint::Inequal) {
                                mergable = true;
                            }
                        }

                        // otherwise if we have one of our inequalities in the prev delta then we can merge
                        // Example: 1000->1011->1021 w/ prevDelta = 0011 delta = 0020
                        for (size_t i = 0; i < delta.arity(); ++i) {
                            if (delta[i] == AttributeConstraint::Inequal) {
                                for (size_t j = 0; j < prevDelta.arity(); ++j) {
                                    if (prevDelta[j] == AttributeConstraint::Equal) {
                                        mergable = true;
                                    }
                                }
                            }
                        }

                        if (!mergable) {
                            successfulMerge = false;
                            break;
                        }

                        // otherwise we merge chains!
                        if (SearchSignature::isSubset(leftDischarged, *right)) {
                            mergedChain.push_back(*left);
                            ++left;
                            continue;
                        }

                        if (SearchSignature::isSubset(rightDischarged, *left)) {
                            mergedChain.push_back(*right);
                            ++right;
                            continue;
                        }
                    }
                }

                // failed to merge so find another pair of chains
                if (!successfulMerge) {
                    continue;
                }

                // if left chain is exhausted then merge the rest of right chain
                if (left == lhs.end()) {
                    while (right != rhs.end()) {
                        mergedChain.push_back(*right);
                        ++right;
                    }
                }
                // if right chain is exhuasted then merge the rest of left chain
                if (right == rhs.end()) {
                    while (left != lhs.end()) {
                        mergedChain.push_back(*left);
                        ++left;
                    }
                }

                changed = true;

                // remove previous 2 chains
                chains.erase(lhs_it);
                chains.erase(rhs_it);

                // insert merged chain
                chains.push_back(mergedChain);
            }
        }
    }

    return chains;
}

MinIndexSelection::AttributeSet MinIndexSelection::getAttributesToDischarge(
        const SearchSignature& s, const RamRelation& rel) {
    // by default we have all attributes w/inequalities discharged
    AttributeSet attributesToDischarge;

    for (size_t i = 0; i < s.arity(); ++i) {
        if (s[i] == AttributeConstraint::Inequal) {
            attributesToDischarge.insert(i);
        }
    }

    // if we don't have a btree then we don't retain any inequalities
    if (rel.getRepresentation() != RelationRepresentation::BTREE &&
            rel.getRepresentation() != RelationRepresentation::DEFAULT) {
        return attributesToDischarge;
    }
    if (Global::config().has("provenance")) {
        return attributesToDischarge;
    }

    auto chains = getAllChains();
    // find the chain that the operation lives inside
    for (auto chain : chains) {
        // get the last operation in the chain
        auto end = *chain.rbegin();
        // if the current operation is this one then we can permit a single indexed inequality
        if (end == s) {
            for (size_t i = 0; i < s.arity(); ++i) {
                // don't discharge an inequality if we have a numeric attribute
                if (s[i] == AttributeConstraint::Inequal) {
                    std::string type = rel.getAttributeTypes()[i];
                    if (type[0] == 'i' || type[0] == 'u' || type[0] == 'f') {
                        // if this is an inequality then it won't be discharged
                        attributesToDischarge.erase(i);
                        break;  // we break here so as to only permit a single indexed inequality
                    }
                }
            }
            break;  // we only care about the chain that the operation belongs to
        }
    }

    return attributesToDischarge;
}

void RamIndexAnalysis::run(const RamTranslationUnit& translationUnit) {
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
    visitDepthFirst(translationUnit.getProgram(), [&](const RamNode& node) {
        if (const auto* indexSearch = dynamic_cast<const RamIndexOperation*>(&node)) {
            MinIndexSelection& indexes = getIndexes(indexSearch->getRelation());
            indexes.addSearch(getSearchSignature(indexSearch));
        } else if (const auto* exists = dynamic_cast<const RamExistenceCheck*>(&node)) {
            MinIndexSelection& indexes = getIndexes(exists->getRelation());
            indexes.addSearch(getSearchSignature(exists));
        } else if (const auto* provExists = dynamic_cast<const RamProvenanceExistenceCheck*>(&node)) {
            MinIndexSelection& indexes = getIndexes(provExists->getRelation());
            indexes.addSearch(getSearchSignature(provExists));
        } else if (const auto* ramRel = dynamic_cast<const RamRelation*>(&node)) {
            MinIndexSelection& indexes = getIndexes(*ramRel);
            indexes.addSearch(getSearchSignature(ramRel));
        }
    });

    // A swap happen between rel A and rel B indicates A should include all indices of B, vice versa.
    visitDepthFirst(translationUnit.getProgram(), [&](const RamSwap& swap) {
        // Note: this naive approach will not work if there exists chain or cyclic swapping.
        // e.g.  swap(relA, relB) swap(relB, relC) swap(relC, relA)
        // One need to keep merging the search set until a fixed point where no more index is introduced
        // in any of the relation in a complete iteration.
        //
        // Currently RAM does not have such situation.
        const RamRelation& relA = swap.getFirstRelation();
        const RamRelation& relB = swap.getSecondRelation();

        MinIndexSelection& indexesA = getIndexes(relA);
        MinIndexSelection& indexesB = getIndexes(relB);
        // Add all searchSignature of A into B
        for (const auto& signature : indexesA.getSearches()) {
            indexesB.addSearch(signature);
        }

        // Add all searchSignature of B into A
        for (const auto& signature : indexesB.getSearches()) {
            indexesA.addSearch(signature);
        }
    });

    // find optimal indexes for relations
    for (auto& cur : minIndexCover) {
        MinIndexSelection& indexes = cur.second;
        indexes.solve();
    }

    // Only case where indexSet is still empty is when relation has arity == 0
    for (auto& cur : minIndexCover) {
        MinIndexSelection& indexes = cur.second;
        if (indexes.getAllOrders().empty()) {
            indexes.insertDefaultTotalIndex(0);
        }
    }
}

MinIndexSelection& RamIndexAnalysis::getIndexes(const RamRelation& rel) {
    auto pos = minIndexCover.find(&rel);
    if (pos != minIndexCover.end()) {
        return pos->second;
    } else {
        auto ret = minIndexCover.insert(std::make_pair(&rel, MinIndexSelection()));
        assert(ret.second);
        return ret.first->second;
    }
}

void RamIndexAnalysis::print(std::ostream& os) const {
    for (auto& cur : minIndexCover) {
        const RamRelation& rel = *cur.first;
        const MinIndexSelection& indexes = cur.second;
        const std::string& relName = rel.getName();

        /* Print searches */
        os << "Relation " << relName << "\n";
        os << "\tNumber of Searches: " << indexes.getSearches().size() << "\n";

        const auto& attrib = rel.getAttributeNames();

        /* print searches */
        for (auto& search : indexes.getSearches()) {
            os << "\t\t";
            os << search;
            os << "\n";
        }

        os << "\tNumber of Indexes: " << indexes.getAllOrders().size() << "\n";
        for (auto& order : indexes.getAllOrders()) {
            os << "\t\t";
            for (auto& i : order) {
                os << attrib[i] << " ";
            }
            os << "\n";
        }
    }
}

namespace {
// handles equality constraints
template <typename Iter>
SearchSignature searchSignature(size_t arity, Iter const& bgn, Iter const& end) {
    SearchSignature keys(arity);

    size_t i = 0;
    for (auto cur = bgn; cur != end; ++cur, ++i) {
        if (!isRamUndefValue(*cur)) {
            keys[i] = AttributeConstraint::Equal;
        }
    }
    return keys;
}

template <typename Seq>
SearchSignature searchSignature(size_t arity, Seq const& xs) {
    return searchSignature(arity, xs.begin(), xs.end());
}
}  // namespace

SearchSignature RamIndexAnalysis::getSearchSignature(const RamIndexOperation* search) const {
    size_t arity = search->getRelation().getArity();

    auto lower = search->getRangePattern().first;
    auto upper = search->getRangePattern().second;
    SearchSignature keys(arity);
    for (size_t i = 0; i < arity; ++i) {
        // if both bounds are undefined
        if (isRamUndefValue(lower[i]) && isRamUndefValue(upper[i])) {
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

SearchSignature RamIndexAnalysis::getSearchSignature(
        const RamProvenanceExistenceCheck* provExistCheck) const {
    const auto values = provExistCheck->getValues();
    auto auxiliaryArity = provExistCheck->getRelation().getAuxiliaryArity();

    SearchSignature keys(values.size());

    // all payload attributes should be equalities
    for (size_t i = 0; i < values.size() - auxiliaryArity; i++) {
        if (!isRamUndefValue(values[i])) {
            keys[i] = AttributeConstraint::Equal;
        }
    }

    // all auxiliary attributes should be free
    for (size_t i = values.size() - auxiliaryArity; i < values.size(); i++) {
        keys[i] = AttributeConstraint::None;
    }

    return keys;
}

SearchSignature RamIndexAnalysis::getSearchSignature(const RamExistenceCheck* existCheck) const {
    return searchSignature(existCheck->getRelation().getArity(), existCheck->getValues());
}

SearchSignature RamIndexAnalysis::getSearchSignature(const RamRelation* ramRel) const {
    return SearchSignature::getFullSearchSignature(ramRel->getArity());
}

bool RamIndexAnalysis::isTotalSignature(const RamAbstractExistenceCheck* existCheck) const {
    for (const auto& cur : existCheck->getValues()) {
        if (isRamUndefValue(cur)) {
            return false;
        }
    }
    return true;
}

}  // end of namespace souffle
