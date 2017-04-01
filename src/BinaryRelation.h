/*
    Copyright (c) 2017 The Souffle Developers and/or its affiliates. All Rights reserved

    The Universal Permissive License (UPL), Version 1.0

    Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of
   this software,
    associated documentation and/or data (collectively the "Software"), free of charge and under any and all
   copyright rights in the
    Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering
   either (i) the unmodified
    Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to
   deal in both

    (a) the Software, and
    (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the
   Software (each a “Larger
    Work” to which the Software is contributed by such licensors),

    without restriction, including without limitation the rights to copy, create derivative works of, display,
   perform, and
    distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the
   Software and the
    Larger Work(s), and to sublicense the foregoing rights on either these or other terms.

    This license is subject to the following condition:
    The above copyright notice and either this complete permission notice or at a minimum a reference to the
   UPL must be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
   LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR
    IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "Trie.h"
#include "UnionFind.h"
#include <unordered_map>
#include <utility>

namespace souffle {
template <typename TupleType>
class BinaryRelation {
    typedef typename TupleType::value_type DomainInt;
    enum { arity = TupleType::arity };

    // Although breaking styleguide - this is marked mutable in order to ensure const-ness fits the other data
    // types
    // It is necessary as "read-only" operations such as iterator generation collapses the disjoint set tree
    // implicitly (causing writes)
    mutable SparseDisjointSet<DomainInt> sds;

    // read/write lock on orderedStates
    mutable souffle::shared_mutex statesLock;

    // the ordering of states per disjoint set (mapping from representative to trie)
    mutable std::unordered_map<DomainInt, std::shared_ptr<souffle::Trie<1>>> orderedStates;

public:
    BinaryRelation& operator=(const BinaryRelation& old) {
        if (this == &old) return *this;

        sds = old.sds;
        orderedStates.clear();

        return *this;
    }

    /**
     * TODO: implement this operation_hint class
     * A collection of operation hints speeding up some of the involved operations
     * by exploiting temporal locality.
     */
    struct operation_hints {
        // resets all hints (to be triggered e.g. when deleting nodes)
        void clear() {}
    };

    /**
     * Insert the two values symbolically as a binary relation
     * @param x node to be added/paired
     * @param y node to be added/paired
     * @return true if the pair is new to the data structure
     */
    bool insert(DomainInt x, DomainInt y) {
        operation_hints z;
        return insert(x, y, z);
    };

    /**
     * Insert the two values symbolically as a binary relation
     * @param x node to be added/paired
     * @param y node to be added/paired
     * @param z the hints to where the pair should be inserted (not applicable atm)
     * @return true if the pair is new to the data structure
     */
    bool insert(DomainInt x, DomainInt y, operation_hints z) {
        // uncomment if race conditions ever present themselves
        // this should only be the case if the tries are generated during insertion
        // and this is not normal souffle behaviour
        // statesLock.lock_shared();

        orderedStates.erase(x);
        orderedStates.erase(y);
        sds.unionNodes(x, y);

        bool retval = contains(x, y);

        // statesLock.unlock_shared();

        return retval;
    }

    /**
     * inserts all nodes from the other relation into this one
     * @param other the binary relation from which to add nodes from
     */
    void insertAll(BinaryRelation<TupleType>& other) {
        // for each representative
        for (auto rep = other.sds.beginReps(); rep != other.sds.endReps(); ++rep) {
            // insert the pairing between the representative and its
            for (auto subrep = other.sds.begin(*rep); subrep != other.sds.end(*rep); ++subrep) {
                this->insert(*rep, *subrep);
            }
        }
    }

    /**
     * Returns whether there exists a pair with these two nodes
     * @param x front of pair
     * @param y back of pair
     */
    bool contains(DomainInt x, DomainInt y) const {
        return sds.contains(x, y);
    }

    void clear() {
        statesLock.lock();

        // we should be able to clear this prior, as it requires a lock on its own
        sds.clear();
        orderedStates.clear();

        statesLock.unlock();
    }

    /**
     * Size of relation
     * @return the sum of the number of pairs per disjoint set
     */
    size_t size() const {
        statesLock.lock_shared();

        size_t retval = 0;
        // sum (n^2)
        for (auto x = sds.beginReps(); x != sds.endReps(); ++x) {
            const size_t sz = sds.sizeOfRepresentativeSet(*x);
            retval += sz * sz;
        }

        statesLock.unlock_shared();

        return retval;
    }

private:
    /**
     * Create a trie which contains the disjoint set which contains this value
     * @param val the value whose disjoint set will be constructed into a trie
     * @return a reference to the newly created trie
     */
    std::shared_ptr<souffle::Trie<1>> generateTrieIfNone(DomainInt val) const {
        if (!sds.nodeExists(val)) throw "cannot generate trie for non-existent node";

        statesLock.lock_shared();
        DomainInt rep = sds.readOnlyFindNode(val);

        // return if already created
        if (this->orderedStates.find(rep) != this->orderedStates.end()) {
            auto retptr = this->orderedStates.at(rep);
            statesLock.unlock_shared();
            return retptr;
        }

        statesLock.unlock_shared();
        statesLock.lock();

        // return if already created (other write thread may have been called simultaneously)
        if (this->orderedStates.find(rep) != this->orderedStates.end()) {
            auto retptr = this->orderedStates.at(rep);
            statesLock.unlock();
            return retptr;
        }

        this->orderedStates[rep] = std::shared_ptr<souffle::Trie<1>>(new souffle::Trie<1>);

        // populate the trie
        for (auto it = sds.begin(rep); it != sds.end(rep); ++it) {
            // add the current value of the iterator to the trie
            this->orderedStates.at(rep)->insert(*it);
        }

        auto retptr = this->orderedStates.at(rep);

        statesLock.unlock();

        return retptr;
    }

public:
    class iterator : public std::iterator<std::forward_iterator_tag, TupleType> {
        // special tombstone value to notify that this iter represents the end
        bool isEndVal = false;

        TupleType value;
        const BinaryRelation* br = nullptr;
        // iterate over all pairs, iterate over all starting at, iterate over all starting at & ending at,
        // iterate over all in dj set, iterate over all (x,_) s.t. x in djset
        enum IterType { BASIC, STARTAT, BETWEEN, CLOSURE, FRONTPROD };
        IterType ityp;

        // the tries for each iterator to belong to
        // these are used for iterating through in order
        std::list<std::pair<std::shared_ptr<souffle::Trie<1>>, souffle::Trie<1>::iterator>> iterList;

        souffle::Trie<1>::iterator frontIter;
        souffle::Trie<1>::iterator backIter;
        std::shared_ptr<souffle::Trie<1>> cTrie;
        // cache the values for cTrie->end(), as it is called... a lot.
        std::unordered_map<std::shared_ptr<souffle::Trie<1>>, souffle::Trie<1>::iterator> cTrieEnds;

        // for iterating between two points
        TupleType endPoint;

        // for the closure
        DomainInt rep;
        // for the front product iter
        std::list<DomainInt> fronts;

    public:
        // ctor for end()
        iterator(bool truthy, const BinaryRelation* br) : isEndVal(true), br(br){};

        // ctor for begin(...)
        iterator(const BinaryRelation* br) : br(br) {
            ityp = BASIC;
            initIterators();
            initCheckEnd();
            setValue();
        }

        // ctor for find(..)
        iterator(const BinaryRelation* br, TupleType& start) : br(br) {
            ityp = STARTAT;

            if (!br->sds.nodeExists(start[0]) || !br->sds.nodeExists(start[1])) {
                isEndVal = true;
                return;
            }

            initIterators();
            ffIterators(start);
            initCheckEnd();
            setValue();
        };

        // ctor for findBetween(..)
        iterator(const BinaryRelation* br, TupleType& start, TupleType& end) : br(br), endPoint(end) {
            ityp = BETWEEN;

            if (!br->sds.nodeExists(start[0]) || !br->sds.nodeExists(start[1])) {
                isEndVal = true;
                return;
            }

            initIterators();
            ffIterators(start);
            initCheckEnd();
            setValue();
        }

        // ctor for closure
        iterator(const BinaryRelation* br, DomainInt rep, std::shared_ptr<souffle::Trie<1>> trie)
                : br(br), rep(rep) {
            ityp = CLOSURE;
            if (!br->sds.nodeExists(rep)) {
                isEndVal = true;
                return;
            }

            initIterator(trie);
            setValue();
        }

        // ctor for front product aka R(x, _) for all x in fronts
        iterator(
                const BinaryRelation* br, std::list<DomainInt> fronts, std::shared_ptr<souffle::Trie<1>> trie)
                : br(br), fronts(fronts) {
            if (std::any_of(
                        fronts.begin(), fronts.end(), [&](DomainInt n) { return !br->sds.nodeExists(n); }))
                throw "non-existent nodes provided";
            ityp = FRONTPROD;
            initIterator(trie);
            // fast forward iter to the first requirement
            auto f = this->fronts.front();
            this->fronts.pop_front();
            while ((*frontIter)[0] != f) ++frontIter;
            setValue();
        }

        /** special fn to set endVal = true
         * only set when the current iterator actually cannot iterate anymore
         * should only be called in the constructor
         */
        void initCheckEnd() {
            if (frontIter == cTrie->end() && backIter == cTrie->end()) isEndVal = true;
        }

        // copy ctor
        iterator(const iterator& other) = default;
        // move ctor
        iterator(iterator&& other) = default;
        // assign iter
        iterator& operator=(const iterator& other) = default;

        bool operator==(const iterator& other) const {
            // return true if we're both end tombstones and of the same br
            if (isEndVal && other.isEndVal) return br == other.br;

            return isEndVal == other.isEndVal && value == other.value;
        }

        bool operator!=(const iterator& other) const {
            return !((*this) == other);
        }

        const TupleType& operator*() const {
            return value;
        }

        const TupleType* operator->() const {
            return &value;
        }

        iterator& operator++() {
            if (isEndVal) throw "error: incrementing an out of range iterator";

            // if we're at the end of our iter, we must jump ahead
            if (backIter == cTrie->end() || ++souffle::Trie<1>::iterator(backIter) == cTrie->end()) {
                // if we cannot do anything more, we quit prematurely, and mark that we've reached the end
                if (advanceFrontIter()) return *this;

                // ^^ the above function also sets backIter to point to the correct point
            } else {
                // we can just step backIter along one
                ++backIter;
            }

            // if its a iterate_until iterator, we should check if the current "value" is > end point
            // if so, mark it as end
            if (ityp == BETWEEN && ((*frontIter)[0] > endPoint[0] || ((*frontIter)[0] == endPoint[0] &&
                                                                             (*backIter)[0] > endPoint[1]))) {
                isEndVal = true;
            } else {
                setValue();
            }

            return *this;
        }

        // TODO: add debugging print statements to match the other souffle data structure abilities

    private:
        /**
         * Returns whether iter has reached the end of cTrie
         */
        bool isTrieEnd(souffle::Trie<1>::iterator iter) {
            return iter == cTrieEnds[cTrie];
        }

        /**
         * Helper function to make the frontIter jump to the next valid Trie & update the current one
         * @return whether we've reached end() or not
         */
        bool advanceFrontIter() {
            // if we're at the end of this current Trie
            if (frontIter == cTrie->end() || ++souffle::Trie<1>::iterator(frontIter) == cTrie->end()) {
                // reaching the end of this trie means that the closure has completed
                if (ityp == CLOSURE || ityp == FRONTPROD) {
                    isEndVal = true;
                    return true;
                }

                // remove this trie from iterList as it means we've exhausted it
                iterList.remove_if(
                        [&](std::pair<std::shared_ptr<souffle::Trie<1>>, souffle::Trie<1>::iterator> x) {
                            return x.first == cTrie;
                        });

                // have we exhausted all other tries?
                if (iterList.empty()) {
                    isEndVal = true;
                    return true;
                }

                // find the next smallest Trie
                bool isStart = true;
                std::pair<std::shared_ptr<souffle::Trie<1>>, souffle::Trie<1>::iterator>* ref = nullptr;

                for (auto& x : iterList) {
                    if (isStart || (*x.second)[0] < (*frontIter)[0]) {
                        ref = &x;
                        cTrie = x.first;
                        frontIter = souffle::Trie<1>::iterator(x.second);
                        isStart = false;
                    }
                }
                // as we've made frontIter point to this, we move our iter to the next one in the Trie
                ++ref->second;
                backIter = cTrie->begin();

            } else {
                if (ityp == FRONTPROD) {
                    // no more front closure?
                    if (fronts.empty()) {
                        isEndVal = true;
                        return true;
                    }

                    // step frontIter until we hit the next valid fronts
                    auto newFront = fronts.front();
                    fronts.pop_front();
                    do {
                        ++frontIter;
                    } while ((*frontIter)[0] != newFront);

                } else {
                    // we can just step frontIter along one, because it will not step past the end of a trie
                    ++frontIter;
                    // step the front reference pointer one along
                    for (auto& x : iterList) {
                        if (x.first == cTrie) {
                            ++x.second;
                            break;
                        }
                    }
                }

                backIter = cTrie->begin();
            }

            // we live to yield another time
            return false;
        }

        /** yield the current iterator value to update to what the iterator is currently pointing at */
        void setValue() {
            if (!isEndVal) {
                TupleType tmp;
                tmp[0] = (*frontIter)[0];
                tmp[1] = (*backIter)[0];

                value = tmp;
            }
        }

        /**
         * Adjust the iterators to point to the beginning of each trie
         */
        void initIterators() {
            bool start = true;

            for (auto& x : br->orderedStates) {
                auto iterBeg = x.second->begin();

                // find the smallest front element of the Tries to set the frontIter to
                if (start) {
                    frontIter = souffle::Trie<1>::iterator(x.second->begin());
                    backIter = souffle::Trie<1>::iterator(frontIter);
                    cTrie = x.second;
                    start = false;
                } else if ((*iterBeg)[0] <= (*frontIter)[0]) {
                    // if the current one is smaller, that becomes the smallest iter
                    frontIter = iterBeg;
                    backIter = souffle::Trie<1>::iterator(frontIter);
                    cTrie = x.second;
                }
                iterList.push_back(std::make_pair(x.second, iterBeg));
                // add to hash map
                cTrieEnds[x.second] = x.second->end();
            }
        }

        /**
         * Single Trie Version of initIterators
         * Only initialise one trie iterator
         * @param trie the trie that we point everything towards
         */
        void initIterator(std::shared_ptr<souffle::Trie<1>> trie) {
            frontIter = trie->begin();
            backIter = trie->begin();
            cTrie = trie;
        }

        /**
         * Fast forward the iterators in iterList (and also frontIter and backIter)
         * s.t. they point to positions >= start
         * @param startVal the first pair that the iterators should be fast forwarded to
         */
        void ffIterators(TupleType& startVal) {
            souffle::Trie<1>::iterator smallestFirst;
            bool start = true;

            for (auto& x : iterList) {
                auto checkIter = x.first->begin();
                auto tmpIter = checkIter;
                auto endTrieIter = x.first->end();

                // keep moving iterators forward until we hit something valid
                // this is based off the assumption that all elements in iterList do not begin at .end()
                while ((*x.second)[0] < startVal[0]) {
                    ++x.second;

                    if (x.second == x.first->end()) {
                        // iterList.remove(x);
                        // TODO: erase this trie from list - we should use .erase instead of remove(), as we
                        // shouldn't iterate over the trie again!
                        // prematurely continue the outerLoop, because we shouldn't consider this value
                        goto breakLoop;
                    }
                }

                // check if we have to discard this trie -
                // this happens when the trie that contains startVal[0], does not have a value larger than
                // startVal[1]
                // todo: we are finding the last element inefficiently, should be a faster way to find it
                // faster
                if ((*x.second)[0] == startVal[0]) {
                    while (checkIter != endTrieIter) {
                        // lag the iterator one behind to capture the last element
                        tmpIter = checkIter;
                        ++checkIter;
                    };
                    if ((*tmpIter)[0] < startVal[1]) {
                        ++x.second;

                        if (x.second == x.first->end()) {
                            // iterList.erase(x);
                            // TODO: erase this trie from list - we should use .erase instead of remove(), as
                            // we shouldn't iterate over the trie again!
                            // prematurely continue the outerLoop, because we shouldn't consider this value
                            goto breakLoop;
                        }
                    }
                }

                // update the frontIter to point to the front most trie now
                if (start || ((*x.second) < *smallestFirst)) {
                    smallestFirst = souffle::Trie<1>::iterator(x.second);
                    cTrie = x.first;
                    start = false;
                }

            breakLoop:;
            }

            frontIter = smallestFirst;
            backIter = cTrie->begin();
            // fast forward the back iterator to point to its starting element
            if ((*frontIter)[0] == startVal[0]) {
                while ((*backIter)[0] < startVal[1]) ++backIter;
            }
        }
    };

public:
    /**
     * iterator pointing to the beginning of the tuples, with no restrictions
     * @return the iterator that corresponds to the beginning of the binary relation
     */
    iterator begin() const {
        // generate tries for all disjoint sets
        for (auto rep = sds.beginReps(); rep != sds.endReps(); ++rep) {
            generateTrieIfNone(*rep);
        }

        return iterator(this);
    }

    /**
     * iterator pointing to the end of the tuples
     * @return the iterator which represents the end of the binary rel
     */
    iterator end() const {
        return iterator(true, this);
    }

    /**
     * Begin an iterator at the requested point
     * @param start where the returned iterator should start from
     * @return the iterator which starts at the requested element
     */
    iterator find(TupleType& start) const {
        // generate tries for all disjoint sets
        // std::for_each(sds.beginReps(), sds.endReps(), [&](DomainInt r) { generateTrieIfNone(r); });

        for (auto rep = sds.beginReps(); rep != sds.endReps(); ++rep) {
            generateTrieIfNone(*rep);
        }

        return iterator(this, start);
    }

    /**
     * Begin an iterator at/after the requested point, and mark it to finish at/before the specified one
     * @param start the requested beginning to iterate from
     * @param end the requested end to iterate until
     * @return the resulting iterator that satisfies this
     */
    iterator findBetween(TupleType& start, TupleType& end) const {
        // generate tries for all disjoint sets
        std::for_each(sds.beginReps(), sds.endReps(), [&](DomainInt r) { generateTrieIfNone(r); });

        return iterator(this, start, end);
    }

    /**
     * Begin an iterator which generates all pairs (X, Y) s.t. X in start & Y in DjSet(X)
     *      If that wasn't clear, so let's say there's an element e in start
     *      The iterator will generate the pairs (e, a), (e, b), ... (e, n) - assuming the elements a,b...n
     * are in e's disjoint set
     * Requires that all elements in start are within the same disjoint set (and in order)
     *      Changing this requires extensive changes to the underlying iterator(BinaryRelation*,
     * std::vector<>) ctor
     * @param start a list of front elements that must exist at the front pairing generated
     * @return the resulting iterator that satisfies this
     */
    iterator frontProduct(std::list<DomainInt> start) const {
        if (start.size() == 0) throw "invalid sized vector for front product";

        auto rep = sds.readOnlyFindNode(start.front());
        if (std::any_of(start.begin(), start.end(),
                    [&](DomainInt i) { return sds.readOnlyFindNode(i) != rep; })) {
            throw "elements not within same disjoint set";
        }

        if (!std::is_sorted(start.begin(), start.end())) {
            for (auto x : start) {
                std::cout << x << '\t';
            }
            std::cout << std::endl;

            throw "elements are not sorted";
        }
        // the trie that contains every value in start
        auto trie = generateTrieIfNone(rep);

        return iterator(this, start, trie);
    }

    /**
     * Begin an iterator over all pairs within a single disjoint set
     * @param rep the representative of (or element within) a disjoint set of which to generate all pairs
     * @return an iterator that will generate all pairs within the disjoint set
     */
    iterator closure(DomainInt rep) const {
        // the trie that contains this rep
        auto trie = generateTrieIfNone(rep);

        return iterator(this, rep, trie);
    }

    /**
     * Generate an approximate number of iterators for parallel iteration
     * The iterators returned are not necessarily equal in size, but in practise are approximately similarly
     * sized
     * Depending on the structure of the data, there can be more or less partitions returned than requested.
     * @param chunks the number of requested partitions
     * @return a list of the iterators as ranges
     */
    std::vector<souffle::range<iterator>> partition(size_t chunks) const {
        std::vector<souffle::range<iterator>> ret;

        for (auto rep = sds.beginReps(); rep != sds.endReps(); ++rep) {
            generateTrieIfNone(*rep);
        }
        // num pairs
        const size_t sz = this->size();

        // 0 or 1 chunks
        if (chunks <= 1 || sz == 0) return {souffle::make_range(begin(), end())};

        // how many pairs can we fit within each iterator? (integer ceil division)
        const size_t chunkSize = (sz + (chunks - 1)) / chunks;

        for (auto djSet = sds.beginReps(); djSet != sds.endReps(); ++djSet) {
            const DomainInt rep = *djSet;

            const size_t djSetSize = sds.sizeOfRepresentativeSet(rep);
            size_t cSize = 0;

            // will the entire djSet's pairs fit within a single iterator?
            if (djSetSize * djSetSize <= chunkSize) {
                // fit all pairs within this disjoint set into a single iterator
                ret.push_back(souffle::make_range(closure(rep), end()));

            } else {
                // add as many of this djSet closures into each iterator
                std::list<DomainInt> fronts;

                for (auto el = sds.begin(rep); el != sds.end(rep); ++el) {
                    fronts.push_back(*el);
                    cSize += djSetSize;

                    // iterator is full now? push this iterator set onto the return val
                    if (cSize >= chunkSize) {
                        fronts.sort();

                        ret.push_back(souffle::make_range(frontProduct(fronts), end()));

                        fronts.clear();

                        cSize = 0;
                    }
                }
                // if there's any remainder still
                if (cSize != 0) ret.push_back(souffle::make_range(frontProduct(fronts), end()));
            }
        }
        return ret;
    }
};
}
