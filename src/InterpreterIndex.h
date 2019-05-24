/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterIndex.h
 *
 * An index is implemented either as a hash-index, a double-hash, as a
 * red-black tree or as a b-tree. The choice of the implementation is
 * set by preprocessor defines.
 *
 ***********************************************************************/

#pragma once

#include <utility>

#include "BTree.h"
#include "RamTypes.h"
#include "Util.h"

namespace souffle {

/* B-Tree indexes as default implementation for indexes */
class InterpreterIndex {
    using LexOrder = std::vector<int>;

public:
    /* lexicographical comparison operation on two tuple pointers */
    struct comparator {
        const LexOrder order;

        /* constructor to initialize state */
        comparator(LexOrder order) : order(std::move(order)) {}

        /* comparison function */
        int operator()(const RamDomain* x, const RamDomain* y) const {
            for (int i : order) {
                if (x[i] < y[i]) {
                    return -1;
                }
                if (x[i] > y[i]) {
                    return 1;
                }
            }
            return 0;
        }

        /* less comparison */
        bool less(const RamDomain* x, const RamDomain* y) const {
            return operator()(x, y) < 0;
        }

        /* equal comparison */
        bool equal(const RamDomain* x, const RamDomain* y) const {
            for (int i : order) {
                if (x[i] != y[i]) {
                    return false;
                }
            }
            return true;
        }
    };

    /* btree for storing tuple pointers with a given lexicographical order */
    using index_set = btree_multiset<const RamDomain*, comparator, std::allocator<const RamDomain*>, 512>;

    using iterator = index_set::iterator;

    InterpreterIndex(LexOrder order)
            : theOrder(std::move(order)), set(comparator(theOrder), comparator(theOrder)) {}

    InterpreterIndex(const InterpreterIndex&& index)
            : theOrder(std::move(index.theOrder)), set(std::move(index.set)) {}

    const LexOrder& order() const {
        return theOrder;
    }

    /**
     * add tuple to the index
     *
     * precondition: tuple does not exist in the index
     */
    void insert(const RamDomain* tuple) {
        set.insert(tuple, operation_hints);
    }

    /**
     * add tuples to the index via an iterator
     *
     * precondition: the tuples do not exist in the index
     */
    template <class Iter>
    void insert(const Iter& a, const Iter& b) {
        set.insert(a, b);
    };

    /** check whether tuple exists in index */
    bool exists(const RamDomain* value) {
        return set.find(value, operation_hints) != set.end();
    }

    /** purge all hashes of index */
    void purge() {
        set.clear();
        operation_hints.clear();
    }

    /** enables the index to be printed */
    void print(std::ostream& out) const {
        set.printStats(out);
        out << "\n";
        set.printTree(out);
    }

    /** return start and end iterator of an equal range */
    inline std::pair<iterator, iterator> equalRange(const RamDomain* value) {
        return lowerUpperBound(value, value);
    }

    /** return start and end iterator of a range */
    inline std::pair<iterator, iterator> lowerUpperBound(const RamDomain* low, const RamDomain* high) {
        return std::pair<iterator, iterator>(
                set.lower_bound(low, operation_hints), set.upper_bound(high, operation_hints));
    }

    /** return start and end iterator of the index set */
    inline std::pair<iterator, iterator> getIteratorPair() const {
        return std::pair<iterator, iterator>(set.begin(), set.end());
    }

private:
    /** retain the index order used to construct an object of this class */
    const LexOrder theOrder;

    /** set storing tuple pointers of table */
    index_set set;

    /** Operation hints */
    index_set::btree_operation_hints<1> operation_hints;
};

}  // end of namespace souffle
