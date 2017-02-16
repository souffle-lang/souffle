/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Trie.h
 *
 * This header file contains the implementation for a generic, fixed
 * length integer trie as it is utilized by the compiled souffle executor.
 *
 * Tries trie is utilized to store n-ary tuples of integers. Each level
 * is implemented via a sparse array (also covered by this header file),
 * referencing the following nested level. The leaf level is realized
 * by a sparse bit-map to minimize the memory footprint.
 *
 * Multiple insert operations can be be conducted concurrently on trie
 * structures. So can read-only operations. However, inserts and read
 * operations may not be conducted at the same time.
 *
 ***********************************************************************/

#pragma once

#include "CompiledRamTuple.h"
#include "Util.h"

#include <atomic>
#include <bitset>
#include <cstring>
#include <iterator>

namespace souffle {

namespace detail {

/**
 * A templated functor to obtain default values for
 * unspecified elements of sparse array instances.
 */
template <typename T>
struct default_factory {
    T operator()() const {
        return T();  // just use the default constructor
    }
};

/**
 * A functor representing the identity function.
 */
template <typename T>
struct identity {
    T operator()(T v) const {
        return v;
    }
};

/**
 * A operation to be utilized by the sparse map when merging
 * elements associated to different values.
 */
template <typename T>
struct default_merge {
    /**
     * Merges two values a and b when merging spase maps.
     */
    T operator()(T a, T b) const {
        default_factory<T> def;
        // if a is the default => us b, else stick to a
        return (a != def()) ? a : b;
    }
};

}  // end namespace detail

/**
 * A sparse array simulates an array associating to every element
 * of uint32_t an element of a generic type T. Any non-defined element
 * will be default-initialized utilizing the detail::default_factory
 * functor.
 *
 * Internally the array is organized as a balanced tree. The leaf
 * level of the tree corresponds to the elements of the represented
 * array. Inner nodes utilize individual bits of the indices to reference
 * sub-trees. For efficiency reasons, only the minimal sub-tree required
 * to cover all non-null / non-default values stored in the array is
 * maintained. Furthermore, several levels of nodes are aggreated in a
 * B-tree like fashion to inprove cache utilization and reduce the number
 * of steps required for lookup and insert operations.
 *
 * @tparam T the type of the stored elements
 * @tparam BITS the number of bits consumed per node-level
 *              e.g. if it is set to 3, the resulting tree will be of a degree of
 *              2^3=8, and thus 8 child-pointers will be stored in each inner node
 *              and as many values will be stored in each leaf node.
 * @tparam merge_op the functor to be utilized when merging the content of two
 *              instances of this type.
 * @tparam copy_op a functor to be applied to each stored value when copying an
 *              instance of this array. For instance, this is utilized by the
 *              trie implementation to create a clone of each sub-tree instead
 *              of preserving the original pointer.
 */
template <typename T, unsigned BITS = 6, typename merge_op = detail::default_merge<T>,
        typename copy_op = detail::identity<T>>
class SparseArray {
    typedef uint64_t key_type;

    // some internal constants
    static const int BIT_PER_STEP = BITS;
    static const int NUM_CELLS = 1 << BIT_PER_STEP;
    static const key_type INDEX_MASK = NUM_CELLS - 1;

public:
    // the type utilized for indexing contained elements
    typedef key_type index_type;

    // the type of value stored in this array
    typedef T value_type;

    // the atomic view on stored values
    typedef std::atomic<value_type> atomic_value_type;

private:
    struct Node;

    /**
     * The value stored in a single cell of a inner
     * or leaf node.
     */
    union Cell {
        // an atomic view on the pointer referencing a nested level
        std::atomic<Node*> aptr;

        // a pointer to the nested level (unsynchronized operations)
        Node* ptr;

        // an atomic view on the value stored in this cell (leaf node)
        atomic_value_type avalue;

        // the value stored in this cell (unsynchronized access, leaf node)
        value_type value;
    };

    /**
     * The node type of the internally maintained tree.
     */
    struct Node {
        // a pointer to the parent node (for efficient iteration)
        const Node* parent;
        // the pointers to the child nodes (inner nodes) or the stored values (leaf nodes)
        Cell cell[NUM_CELLS];
    };

    /**
     * A struct describing all the information required by the container
     * class to manage the wrapped up tree.
     */
    struct RootInfo {
        // the root node of the tree
        Node* root;
        // the number of levels of the tree
        uint32_t levels;
        // the absolute offset of the theoretical first element in the tree
        index_type offset;

        // the first leaf node in the tree
        Node* first;
        // the absolute offset of the first element in the first leaf node
        index_type firstOffset;
    };

    union {
        RootInfo unsynced;         // for sequential operations
        volatile RootInfo synced;  // for synchronized operations
    };

public:
    /**
     * A default constructor creating an empty sparse array.
     */
    SparseArray() : unsynced(RootInfo{nullptr, 0, 0, nullptr, std::numeric_limits<index_type>::max()}) {}

    /**
     * A copy constructor for sparse arrays. It creates a deep
     * copy of the data structure maintained by the handed in
     * array instance.
     */
    SparseArray(const SparseArray& other)
            : unsynced(RootInfo{clone(other.unsynced.root, other.unsynced.levels), other.unsynced.levels,
                      other.unsynced.offset, nullptr, other.unsynced.firstOffset}) {
        if (unsynced.root) {
            unsynced.root->parent = nullptr;
            unsynced.first = findFirst(unsynced.root, unsynced.levels);
        }
    }

    /**
     * A r-value based copy constructor for sparse arrays. It
     * takes over ownership of the structure maintained by the
     * handed in array.
     */
    SparseArray(SparseArray&& other)
            : unsynced(RootInfo{other.unsynced.root, other.unsynced.levels, other.unsynced.offset,
                      other.unsynced.first, other.unsynced.firstOffset}) {
        other.unsynced.root = nullptr;
        other.unsynced.levels = 0;
        other.unsynced.first = nullptr;
    }

    /**
     * A destructor for sparse arrays clearing up the internally
     * maintained data structure.
     */
    ~SparseArray() {
        clean();
    }

    /**
     * An assignment creating a deep copy of the handed in
     * array structure (utilizing the copy functor provided
     * as a template parameter).
     */
    SparseArray& operator=(const SparseArray& other) {
        if (this == &other) return *this;

        // clean this one
        clean();

        // copy content
        unsynced.levels = other.unsynced.levels;
        unsynced.root = clone(other.unsynced.root, unsynced.levels);
        if (unsynced.root) unsynced.root->parent = nullptr;
        unsynced.offset = other.unsynced.offset;
        unsynced.first = (unsynced.root) ? findFirst(unsynced.root, unsynced.levels) : nullptr;
        unsynced.firstOffset = other.unsynced.firstOffset;

        // done
        return *this;
    }

    /**
     * An assignment operation taking over ownership
     * from a r-value reference to a sparse array.
     */
    SparseArray& operator=(SparseArray&& other) {
        // clean this one
        clean();

        // harvest content
        unsynced.root = other.unsynced.root;
        unsynced.levels = other.unsynced.levels;
        unsynced.offset = other.unsynced.offset;
        unsynced.first = other.unsynced.first;
        unsynced.firstOffset = other.unsynced.firstOffset;

        // reset other
        other.unsynced.root = nullptr;
        other.unsynced.levels = 0;
        other.unsynced.first = nullptr;

        // done
        return *this;
    }

    /**
     * Tests whether this sparse array is empty, thus it only
     * contains default-values, or not.
     */
    bool empty() const {
        return unsynced.root == nullptr;
    }

    /**
     * Computes the number of non-empty elements within this
     * sparse array.
     */
    std::size_t size() const {
        // quick one for the empty map
        if (empty()) return 0;

        // count elements -- since maintaining is making inserts more expensive
        std::size_t res = 0;
        for (auto it = begin(); it != end(); ++it) {
            ++res;
        }
        return res;
    }

private:
    /**
     * Computes the memory usage of the given sub-tree.
     */
    static std::size_t getMemoryUsage(const Node* node, int level) {
        // support null-nodes
        if (!node) return 0;

        // add size of current node
        std::size_t res = sizeof(Node);

        // sum up memory usage of child nodes
        if (level > 0) {
            for (int i = 0; i < NUM_CELLS; i++) {
                res += getMemoryUsage(node->cell[i].ptr, level - 1);
            }
        }

        // done
        return res;
    }

public:
    /**
     * Computes the total memory usage of this data structure.
     */
    std::size_t getMemoryUsage() const {
        // the memory of the wrapper class
        std::size_t res = sizeof(*this);

        // add nodes
        if (unsynced.root) {
            res += getMemoryUsage(unsynced.root, unsynced.levels);
        }

        // done
        return res;
    }

    /**
     * Resets the content of this array to default values for each contained
     * element.
     */
    void clear() {
        clean();
        unsynced.root = nullptr;
        unsynced.levels = 0;
        unsynced.first = nullptr;
        unsynced.firstOffset = std::numeric_limits<index_type>::max();
    }

    /**
     * A struct to be utilized as a local, temporal context by client code
     * to speed up the execution of various operations (optional parameter).
     */
    struct op_context {
        index_type lastIndex;
        Node* lastNode;
        op_context() : lastIndex(0), lastNode(nullptr) {}
    };

private:
    // ---------------------------------------------------------------------
    //              Optimistic Locking of Root-Level Infos
    // ---------------------------------------------------------------------

    /**
     * A struct to cover a snapshot of the root node state.
     */
    struct RootInfoSnapshot {
        // the current pointer to a root node
        Node* root;
        // the current number of levels
        uint32_t levels;
        // the current offset of the first theoretical element
        index_type offset;
        // a version number for the optimistic locking
        uintptr_t version;
    };

    /**
     * Obtains the current version of the root.
     */
    uint64_t getRootVersion() const {
        // here it is assumed that the load of a 64-bit word is atomic
        return (uint64_t)synced.root;
    }

    /**
     * Obtains a snapshot of the current root information.
     */
    RootInfoSnapshot getRootInfo() const {
        RootInfoSnapshot res;
        do {
            // first take the mod counter
            do {
                // if res.mod % 2 == 1 .. there is an update in progress
                res.version = getRootVersion();
            } while (res.version % 2);

            // then the rest
            res.root = synced.root;
            res.levels = synced.levels;
            res.offset = synced.offset;

            // check consistency of obtained data (optimistic locking)
        } while (res.version != getRootVersion());

        // got a consistent snapshot
        return res;
    }

    /**
     * Updates the current root information based on the handed in modified
     * snapshot instance if the version number of the snapshot still corresponds
     * to the current version. Otherwise a concurrent update took place and the
     * operation is aborted.
     *
     * @param info the updated information to be assigned to the active root-info data
     * @return true if successfully updated, false if aborted
     */
    bool tryUpdateRootInfo(const RootInfoSnapshot& info) {
        // check mod counter
        uintptr_t version = info.version;

        // update root to invalid pointer (ending with 1)
        if (!__sync_bool_compare_and_swap(&synced.root, (Node*)version, (Node*)(version + 1))) {
            return false;
        }

        // conduct update
        synced.levels = info.levels;
        synced.offset = info.offset;

        // update root (and thus the version to enable future retrievals)
        __sync_synchronize();
        synced.root = info.root;

        // done
        return true;
    }

    /**
     * A struct summarizing the state of the first node reference.
     */
    struct FirstInfoSnapshot {
        // the pointer to the first node
        Node* node;
        // the offset of the first node
        index_type offset;
        // the version number of the first node (for the optimistic locking)
        uintptr_t version;
    };

    /**
     * Obtains the current version number of the first node information.
     */
    uint64_t getFirstVersion() const {
        // here it is assumed that the load of a 64-bit word is atomic
        return (uint64_t)synced.first;
    }

    /**
     * Obtains a snapshot of the current first-node information.
     */
    FirstInfoSnapshot getFirstInfo() const {
        FirstInfoSnapshot res;
        do {
            // first take the version
            do {
                res.version = getFirstVersion();
            } while (res.version % 2);

            // collect the values
            res.node = synced.first;
            res.offset = synced.firstOffset;

        } while (res.version != getFirstVersion());

        // we got a consistent snapshot
        return res;
    }

    /**
     * Updates the information stored regarding the first node in a
     * concurrent setting utilizing a optimistic locking approach.
     * This is identical to the approach utilized for the root info.
     */
    bool tryUpdateFirstInfo(const FirstInfoSnapshot& info) {
        // check mod counter
        uintptr_t version = info.version;

        // temporary update first pointer to point to uneven value (lock-out)
        if (!__sync_bool_compare_and_swap(&synced.first, (Node*)version, (Node*)(version + 1))) {
            return false;
        }

        // conduct update
        synced.firstOffset = info.offset;

        // update node pointer (and thus the version number)
        __sync_synchronize();
        synced.first = info.node;  // must be last (and atomic)

        // done
        return true;
    }

public:
    /**
     * Obtains a mutable reference to the value addressed by the given index.
     *
     * @param i the index of the element to be addressed
     * @return a mutable reference to the corresponding element
     */
    value_type& get(index_type i) {
        op_context ctxt;
        return get(i, ctxt);
    }

    /**
     * Obtains a mutable reference to the value addressed by the given index.
     *
     * @param i the index of the element to be addressed
     * @param ctxt a operation context to exploit state-less temporal locality
     * @return a mutable reference to the corresponding element
     */
    value_type& get(index_type i, op_context& ctxt) {
        return getLeaf(i, ctxt).value;
    }

    /**
     * Obtains a mutable reference to the atomic value addressed by the given index.
     *
     * @param i the index of the element to be addressed
     * @return a mutable reference to the corresponding element
     */
    atomic_value_type& getAtomic(index_type i) {
        op_context ctxt;
        return getAtomic(i, ctxt);
    }

    /**
     * Obtains a mutable reference to the atomic value addressed by the given index.
     *
     * @param i the index of the element to be addressed
     * @param ctxt a operation context to exploit state-less temporal locality
     * @return a mutable reference to the corresponding element
     */
    atomic_value_type& getAtomic(index_type i, op_context& ctxt) {
        return getLeaf(i, ctxt).avalue;
    }

private:
    /**
     * An internal function capable of navigating to a given leaf node entry.
     * If the cell does not exist yet it will be created as a side-effect.
     *
     * @param i the index of the requested cell
     * @param ctxt a operation context to exploit state-less temporal locality
     * @return a reference to the requested cell
     */
    inline Cell& getLeaf(index_type i, op_context& ctxt) {
        // check context
        if (ctxt.lastNode && (ctxt.lastIndex == (i & ~INDEX_MASK))) {
            // return reference to referenced
            return ctxt.lastNode->cell[i & INDEX_MASK];
        }

        // get snapshot of root
        auto info = getRootInfo();

        // check for emptiness
        if (info.root == nullptr) {
            // build new root node
            info.root = newNode();

            // initialize the new node
            info.root->parent = nullptr;
            info.offset = i & ~(INDEX_MASK);

            // try updating root information atomically
            if (tryUpdateRootInfo(info)) {
                // success -- finish get call

                // update first
                auto firstInfo = getFirstInfo();
                while (info.offset < firstInfo.offset) {
                    firstInfo.node = info.root;
                    firstInfo.offset = info.offset;
                    if (!tryUpdateFirstInfo(firstInfo)) {
                        // there was some concurrent update => check again
                        firstInfo = getFirstInfo();
                    }
                }

                // return reference to proper cell
                return info.root->cell[i & INDEX_MASK];
            }

            // somebody else was faster => use standard insertion procedure
            free(info.root);

            // retrieve new root info
            info = getRootInfo();

            // make sure there is a root
            assert(info.root);
        }

        // for all other inserts
        //   - check boundary
        //   - navigate to node
        //   - insert value

        // check boundaries
        while (!inBoundaries(i, info.levels, info.offset)) {
            // boundaries need to be expanded by growing upwards
            raiseLevel(info);  // try raising level unless someone else did already
            // update root info
            info = getRootInfo();
        }

        // navigate to node
        Node* node = info.root;
        unsigned level = info.levels;
        while (level != 0) {
            // get X coordinate
            auto x = getIndex(i, level);

            // decrease level counter
            --level;

            // check next node
            std::atomic<Node*>& aNext = node->cell[x].aptr;
            Node* next = aNext;
            if (!next) {
                // create new sub-tree
                Node* newNext = newNode();
                newNext->parent = node;

                // try to update next
                if (!aNext.compare_exchange_strong(next, newNext)) {
                    // some other thread was faster => use updated next
                    free(newNext);
                } else {
                    // the locally created next is the new next
                    next = newNext;

                    // update first
                    if (level == 0) {
                        // compute offset of this node
                        auto off = i & ~INDEX_MASK;

                        // fast over-approximation of whether a update is necessary
                        if (off < unsynced.firstOffset) {
                            // update first reference if this one is the smallest
                            auto info = getFirstInfo();
                            while (off < info.offset) {
                                info.node = next;
                                info.offset = off;
                                if (!tryUpdateFirstInfo(info)) {
                                    // there was some concurrent update => check again
                                    info = getFirstInfo();
                                }
                            }
                        }
                    }
                }

                // now next should be defined
                assert(next);
            }

            // continue one level below
            node = next;
        }

        // update context
        ctxt.lastIndex = (i & ~INDEX_MASK);
        ctxt.lastNode = node;

        // return reference to cell
        return node->cell[i & INDEX_MASK];
    }

public:
    /**
     * Updates the value stored in cell i by the given value.
     */
    void update(index_type i, const value_type& val) {
        op_context ctxt;
        update(i, val, ctxt);
    }

    /**
     * Updates the value stored in cell i by the given value. A operation
     * context can be provided for exploiting temporal locality.
     */
    void update(index_type i, const value_type& val, op_context& ctxt) {
        get(i, ctxt) = val;
    }

    /**
     * Obtains the value associated to index i -- which might be
     * the default value of the covered type if the value hasn't been
     * defined previously.
     */
    value_type operator[](index_type i) const {
        return lookup(i);
    }

    /**
     * Obtains the value associated to index i -- which might be
     * the default value of the covered type if the value hasn't been
     * defined previously.
     */
    value_type lookup(index_type i) const {
        op_context ctxt;
        return lookup(i, ctxt);
    }

    /**
     * Obtains the value associated to index i -- which might be
     * the default value of the covered type if the value hasn't been
     * defined previously. A operation context can be provided for
     * exploiting temporal locality.
     */
    value_type lookup(index_type i, op_context& ctxt) const {
        // check whether it is empty
        if (!unsynced.root) return detail::default_factory<value_type>()();

        // check boundaries
        if (!inBoundaries(i)) return detail::default_factory<value_type>()();

        // check context
        if (ctxt.lastNode && ctxt.lastIndex == (i & ~INDEX_MASK)) {
            return ctxt.lastNode->cell[i & INDEX_MASK].value;
        }

        // navigate to value
        Node* node = unsynced.root;
        unsigned level = unsynced.levels;
        while (level != 0) {
            // get X coordinate
            auto x = getIndex(i, level);

            // decrease level counter
            --level;

            // check next node
            Node* next = node->cell[x].ptr;

            // check next step
            if (!next) return detail::default_factory<value_type>()();

            // continue one level below
            node = next;
        }

        // remember context
        ctxt.lastIndex = (i & ~INDEX_MASK);
        ctxt.lastNode = node;

        // return reference to cell
        return node->cell[i & INDEX_MASK].value;
    }

private:
    /**
     * A static operation utilized internally for merging sub-trees recursively.
     *
     * @param parent the parent node of the current merge operation
     * @param trg a reference to the pointer the cloned node should be stored to
     * @param src the node to be cloned
     * @param levels the height of the cloned node
     */
    static void merge(const Node* parent, Node*& trg, const Node* src, int levels) {
        // if other side is null => done
        if (!src) return;

        // if the trg sub-tree is empty, clone the corresponding branch
        if (trg == nullptr) {
            trg = clone(src, levels);
            if (trg) trg->parent = parent;
            return;  // done
        }

        // otherwise merge recursively

        // the leaf-node step
        if (levels == 0) {
            merge_op merg;
            for (int i = 0; i < NUM_CELLS; ++i) {
                trg->cell[i].value = merg(trg->cell[i].value, src->cell[i].value);
            }
            return;
        }

        // the recursive step
        for (int i = 0; i < NUM_CELLS; ++i) {
            merge(trg, trg->cell[i].ptr, src->cell[i].ptr, levels - 1);
        }
    }

public:
    /**
     * Adds all the values stored in the given array to this array.
     */
    void addAll(const SparseArray& other) {
        // skip if other is empty
        if (other.empty()) {
            return;
        }

        // special case: emptiness
        if (empty()) {
            // use assignment operator
            *this = other;
            return;
        }

        // adjust levels
        while (unsynced.levels < other.unsynced.levels || !inBoundaries(other.unsynced.offset)) {
            raiseLevel();
        }

        // navigate to root node equivalent of the other node in this tree
        auto level = unsynced.levels;
        Node** node = &unsynced.root;
        while (level > other.unsynced.levels) {
            // get X coordinate
            auto x = getIndex(other.unsynced.offset, level);

            // decrease level counter
            --level;

            // check next node
            Node*& next = (*node)->cell[x].ptr;
            if (!next) {
                // create new sub-tree
                next = newNode();
                next->parent = *node;
            }

            // continue one level below
            node = &next;
        }

        // merge sub-branches from here
        merge((*node)->parent, *node, other.unsynced.root, level);

        // update first
        if (unsynced.firstOffset > other.unsynced.firstOffset) {
            unsynced.first = findFirst(*node, level);
            unsynced.firstOffset = other.unsynced.firstOffset;
        }
    }

    // ---------------------------------------------------------------------
    //                           Iterator
    // ---------------------------------------------------------------------

    /**
     * The iterator type to be utilized to iterate over the non-default elements of this array.
     */
    class iterator : public std::iterator<std::forward_iterator_tag, std::pair<index_type, value_type>> {
        typedef std::pair<index_type, value_type> pair_type;

        // a pointer to the leaf node currently processed or null (end)
        const Node* node;

        // the value currently pointed to
        pair_type value;

    public:
        // default constructor -- creating an end-iterator
        iterator() : node(nullptr) {}

        iterator(const Node* node, const pair_type& value) : node(node), value(value) {}

        iterator(const Node* first, index_type firstOffset) : node(first), value(firstOffset, 0) {
            // if the start is the end => we are done
            if (!first) return;

            // load the value
            if (first->cell[0].value == value_type()) {
                ++(*this);  // walk to first element
            } else {
                value.second = first->cell[0].value;
            }
        }

        // a copy constructor
        iterator(const iterator& other) = default;

        // an assignment operator
        iterator& operator=(const iterator& other) = default;

        // the equality operator as required by the iterator concept
        bool operator==(const iterator& other) const {
            // only equivalent if pointing to the end
            return (node == nullptr && other.node == nullptr) ||
                   (node == other.node && value.first == other.value.first);
        }

        // the not-equality operator as required by the iterator concept
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        // the deref operator as required by the iterator concept
        const pair_type& operator*() const {
            return value;
        }

        // support for the pointer operator
        const pair_type* operator->() const {
            return &value;
        }

        // the increment operator as required by the iterator concept
        iterator& operator++() {
            // get current offset
            index_type x = value.first & INDEX_MASK;

            // go to next non-empty value in current node
            do {
                x++;
            } while (x < NUM_CELLS && node->cell[x].value == value_type());

            // check whether one has been found
            if (x < NUM_CELLS) {
                // update value and be done
                value.first = (value.first & ~INDEX_MASK) | x;
                value.second = node->cell[x].value;
                return *this;  // done
            }

            // go to parent
            node = node->parent;
            int level = 1;

            // get current index on this level
            x = getIndex(value.first, level);
            x++;

            while (level > 0 && node) {
                // search for next child
                while (x < NUM_CELLS) {
                    if (node->cell[x].ptr) break;
                    x++;
                }

                // pick next step
                if (x < NUM_CELLS) {
                    // going down
                    node = node->cell[x].ptr;
                    value.first &= getLevelMask(level + 1);
                    value.first |= x << (BIT_PER_STEP * level);
                    level--;
                    x = 0;
                } else {
                    // going up
                    node = node->parent;
                    level++;

                    // get current index on this level
                    x = getIndex(value.first, level);
                    x++;  // go one step further
                }
            }

            // check whether it is the end of range
            if (!node) return *this;

            // search the first value in this node
            x = 0;
            while (node->cell[x].value == value_type()) {
                x++;
            }

            // update value
            value.first |= x;
            value.second = node->cell[x].value;

            // done
            return *this;
        }

        // True if this iterator is passed the last element.
        bool isEnd() const {
            return node == nullptr;
        }

        // enables this iterator core to be printed (for debugging)
        void print(std::ostream& out) const {
            out << "SparseArrayIter(" << node << " @ " << value << ")";
        }

        friend std::ostream& operator<<(std::ostream& out, const iterator& iter) {
            iter.print(out);
            return out;
        }
    };

    /**
     * Obtains an iterator referencing the first non-default element or end in
     * case there are no such elements.
     */
    iterator begin() const {
        return iterator(unsynced.first, unsynced.firstOffset);
    }

    /**
     * An iterator referencing the position after the last non-default element.
     */
    iterator end() const {
        return iterator();
    }

    /**
     * An operation to obtain an iterator referencing an element addressed by the
     * given index. If the corresponding element is a non-default value, a corresponding
     * iterator will be returned. Otherwise end() will be returned.
     */
    iterator find(index_type i) const {
        op_context ctxt;
        return find(i, ctxt);
    }

    /**
     * An operation to obtain an iterator referencing an element addressed by the
     * given index. If the corresponding element is a non-default value, a corresponding
     * iterator will be returned. Otherwise end() will be returned. A operation context
     * can be provided for exploiting temporal locality.
     */
    iterator find(index_type i, op_context& ctxt) const {
        // check whether it is empty
        if (!unsynced.root) return end();

        // check boundaries
        if (!inBoundaries(i)) return end();

        // check context
        if (ctxt.lastNode && ctxt.lastIndex == (i & ~INDEX_MASK)) {
            Node* node = ctxt.lastNode;

            // check whether there is a proper entry
            value_type value = node->cell[i & INDEX_MASK].value;
            if (value == 0) {
                return end();
            }
            // return iterator pointing to value
            return iterator(node, std::make_pair(i, value));
        }

        // navigate to value
        Node* node = unsynced.root;
        unsigned level = unsynced.levels;
        while (level != 0) {
            // get X coordinate
            auto x = getIndex(i, level);

            // decrease level counter
            --level;

            // check next node
            Node* next = node->cell[x].ptr;

            // check next step
            if (!next) return end();

            // continue one level below
            node = next;
        }

        // register in context
        ctxt.lastNode = node;
        ctxt.lastIndex = (i & ~INDEX_MASK);

        // check whether there is a proper entry
        value_type value = node->cell[i & INDEX_MASK].value;
        if (value == 0) {
            return end();
        }

        // return iterator pointing to cell
        return iterator(node, std::make_pair(i, value));
    }

    /**
     * An operation obtaining the smallest non-default element such that it's index is >=
     * the given index.
     */
    iterator lowerBound(index_type i) const {
        op_context ctxt;
        return lowerBound(i, ctxt);
    }

    /**
     * An operation obtaining the smallest non-default element such that it's index is >=
     * the given index. A operation context can be provided for exploiting temporal locality.
     */
    iterator lowerBound(index_type i, op_context&) const {
        // check whether it is empty
        if (!unsynced.root) return end();

        // check boundaries
        if (!inBoundaries(i)) return end();

        // navigate to value
        Node* node = unsynced.root;
        unsigned level = unsynced.levels;
        while (true) {
            // get X coordinate
            auto x = getIndex(i, level);

            // check next node
            Node* next = node->cell[x].ptr;

            // check next step
            if (!next) {
                if (x == NUM_CELLS - 1) {
                    ++level;
                    node = const_cast<Node*>(node->parent);
                    if (!node) return end();
                }

                // continue search
                i = i & getLevelMask(level);

                // find next higher value
                i += (1 << (BITS * level));

            } else {
                if (level == 0) {
                    // found boundary
                    return iterator(node, std::make_pair(i, node->cell[x].value));
                }

                // decrease level counter
                --level;

                // continue one level below
                node = next;
            }
        }
    }

private:
    /**
     * An internal debug utility printing the internal structure of this sparse array to the given output
     * stream.
     */
    void dump(bool detailed, std::ostream& out, const Node& node, int level, index_type offset,
            int indent = 0) const {
        auto x = getIndex(offset, level + 1);
        out << times("\t", indent) << x << ": Node " << &node << " on level " << level
            << " parent: " << node.parent << " -- range: " << offset << " - "
            << (offset + ~getLevelMask(level + 1)) << "\n";

        if (level == 0) {
            for (int i = 0; i < NUM_CELLS; i++) {
                if (detailed || node.cell[i].value != value_type()) {
                    out << times("\t", indent + 1) << i << ": [" << (offset + i) << "] " << node.cell[i].value
                        << "\n";
                }
            }
        } else {
            for (int i = 0; i < NUM_CELLS; i++) {
                if (node.cell[i].ptr) {
                    dump(detailed, out, *node.cell[i].ptr, level - 1,
                            offset + (i * (index_type(1) << (level * BIT_PER_STEP))), indent + 1);
                } else if (detailed) {
                    auto low = offset + (i * (1 << (level * BIT_PER_STEP)));
                    auto hig = low + ~getLevelMask(level);
                    out << times("\t", indent + 1) << i << ": empty range " << low << " - " << hig << "\n";
                }
            }
        }
        out << "\n";
    }

public:
    /**
     * A debug utility printing the internal structure of this sparse array to the given output stream.
     */
    void dump(bool detail = false, std::ostream& out = std::cout) const {
        if (!unsynced.root) {
            out << " - empty - \n";
            return;
        }
        out << "root:  " << unsynced.root << "\n";
        out << "offset: " << unsynced.offset << "\n";
        out << "first: " << unsynced.first << "\n";
        out << "fist offset: " << unsynced.firstOffset << "\n";
        dump(detail, out, *unsynced.root, unsynced.levels, unsynced.offset);
    }

private:
    // --------------------------------------------------------------------------
    //                                 Utilities
    // --------------------------------------------------------------------------

    /**
     * Creates new nodes and initializes them with 0.
     */
    static Node* newNode() {
        Node* res = (Node*)(malloc(sizeof(Node)));
        std::memset(res->cell, 0, sizeof(Cell) * NUM_CELLS);
        return res;
    }

    /**
     * Destroys a node and all its sub-nodes recursively.
     */
    static void freeNodes(Node* node, int level) {
        if (!node) return;
        if (level != 0) {
            for (int i = 0; i < NUM_CELLS; i++) {
                freeNodes(node->cell[i].ptr, level - 1);
            }
        }
        free(node);
    }

    /**
     * Conducts a cleanup of the internal tree structure.
     */
    void clean() {
        freeNodes(unsynced.root, unsynced.levels);
        unsynced.root = nullptr;
        unsynced.levels = 0;
    }

    /**
     * Clones the given node and all its sub-nodes.
     */
    static Node* clone(const Node* node, int level) {
        // support null-pointers
        if (!node) return nullptr;

        // create a clone
        Node* res = (Node*)(malloc(sizeof(Node)));

        // handle leaf level
        if (level == 0) {
            copy_op copy;
            for (int i = 0; i < NUM_CELLS; i++) {
                res->cell[i].value = copy(node->cell[i].value);
            }
            return res;
        }

        // for inner nodes clone each child
        for (int i = 0; i < NUM_CELLS; i++) {
            auto cur = clone(node->cell[i].ptr, level - 1);
            if (cur) cur->parent = res;
            res->cell[i].ptr = cur;
        }

        // done
        return res;
    }

    /**
     * Obtains the left-most leaf-node of the tree rooted by the given node
     * with the given level.
     */
    static Node* findFirst(Node* node, int level) {
        while (level > 0) {
            bool found = false;
            for (int i = 0; i < NUM_CELLS; i++) {
                Node* cur = node->cell[i].ptr;
                if (cur) {
                    node = cur;
                    --level;
                    found = true;
                    break;
                }
            }
            assert(found && "No first node!");
        }

        return node;
    }

    /**
     * Raises the level of this tree by one level. It does so by introducing
     * a new root node and inserting the current root node as a child node.
     */
    void raiseLevel() {
        // something went wrong when we pass that line
        assert(unsynced.levels < (sizeof(index_type) * 8 / BITS) + 1);

        // create new root
        Node* node = newNode();
        node->parent = nullptr;

        // insert existing root as child
        auto x = getIndex(unsynced.offset, unsynced.levels + 1);
        node->cell[x].ptr = unsynced.root;

        // swap the root
        unsynced.root->parent = node;

        // update root
        unsynced.root = node;
        ++unsynced.levels;

        // update offset be removing additional bits
        unsynced.offset &= getLevelMask(unsynced.levels + 1);
    }

    /**
     * Attempts to raise the height of this tree based on the given root node
     * information and updates the root-info snapshot correspondingly.
     */
    void raiseLevel(RootInfoSnapshot& info) {
        // something went wrong when we pass that line
        assert(info.levels < (sizeof(index_type) * 8 / BITS) + 1);

        // create new root
        Node* newRoot = newNode();
        newRoot->parent = nullptr;

        // insert existing root as child
        auto x = getIndex(info.offset, info.levels + 1);
        newRoot->cell[x].ptr = info.root;

        // exchange the root in the info struct
        auto oldRoot = info.root;
        info.root = newRoot;

        // update level counter
        ++info.levels;

        // update offset
        info.offset &= getLevelMask(info.levels + 1);

        // try exchanging root info
        if (tryUpdateRootInfo(info)) {
            // success => final step, update parent of old root
            oldRoot->parent = info.root;
        } else {
            // throw away temporary new node
            free(newRoot);
        }
    }

    /**
     * Tests whether the given index is covered by the boundaries defined
     * by the hight and offset of the internally maintained tree.
     */
    bool inBoundaries(index_type a) const {
        return inBoundaries(a, unsynced.levels, unsynced.offset);
    }

    /**
     * Tests whether the given index is within the boundaries defined by the
     * given tree hight and offset.
     */
    static bool inBoundaries(index_type a, uint32_t levels, index_type offset) {
        auto mask = getLevelMask(levels + 1);
        return (a & mask) == offset;
    }

    /**
     * Obtains the index within the arrays of cells of a given index on a given
     * level of the internally maintained tree.
     */
    static index_type getIndex(RamDomain a, unsigned level) {
        return (a & (INDEX_MASK << (level * BIT_PER_STEP))) >> (level * BIT_PER_STEP);
    }

    /**
     * Computes the bit-mask to be applicable to obtain the offset of a node on a
     * given tree level.
     */
    static index_type getLevelMask(unsigned level) {
        if (level > (sizeof(index_type) * 8 / BITS)) return 0;
        return (~(index_type(0)) << (level * BIT_PER_STEP));
    }
};

/**
 * A sparse bit-map is a bit map virtually assigning a bit value to every value if the
 * uint32_t domain. However, only 1-bits are stored utilizing a nested sparse array
 * structure.
 *
 * @tparam BITS similar to the BITS parameter of the sparse array type
 */
template <unsigned BITS = 4>
class SparseBitMap {
    // the element type stored in the nested sparse array
    typedef uint64_t value_t;

    // define the bit-level merge operation
    struct merge_op {
        value_t operator()(value_t a, value_t b) const {
            return a | b;  // merging bit masks => bitwise or operation
        }
    };

    // the type of the internal data store
    typedef SparseArray<value_t, BITS, merge_op> data_store_t;
    typedef typename data_store_t::atomic_value_type atomic_value_t;

    // some constants for manipulating stored values
    static const short BITS_PER_ENTRY = sizeof(value_t) * 8;
    static const short LEAF_INDEX_WIDTH = __builtin_ctz(BITS_PER_ENTRY);
    static const uint64_t LEAF_INDEX_MASK = BITS_PER_ENTRY - 1;

public:
    // the type to address individual entries
    typedef typename data_store_t::index_type index_type;

private:
    // it utilizes a sparse map to store its data
    data_store_t store;

public:
    // a simple default constructor
    SparseBitMap() {}

    // a default copy constructor
    SparseBitMap(const SparseBitMap&) = default;

    // a default r-value copy constructor
    SparseBitMap(SparseBitMap&&) = default;

    // a default assignment operator
    SparseBitMap& operator=(const SparseBitMap&) = default;

    // a default r-value assignment operator
    SparseBitMap& operator=(SparseBitMap&&) = default;

    // checks whether this bit-map is empty -- thus it does not have any 1-entries
    bool empty() const {
        return store.empty();
    }

    // the type utilized for recording context information for exploiting temporal locality
    typedef typename data_store_t::op_context op_context;

    /**
     * Sets the bit addressed by i to 1.
     */
    bool set(index_type i) {
        op_context ctxt;
        return set(i, ctxt);
    }

    /**
     * Sets the bit addressed by i to 1. A context for exploiting temporal locality
     * can be provided.
     */
    bool set(index_type i, op_context& ctxt) {
        atomic_value_t& val = store.getAtomic(i >> LEAF_INDEX_WIDTH, ctxt);
        value_t bit = (1ull << (i & LEAF_INDEX_MASK));
        value_t old = val.fetch_or(bit, std::memory_order::memory_order_relaxed);
        return !(old & bit);
    }

    /**
     * Determines the whether the bit addressed by i is set or not.
     */
    bool test(index_type i) const {
        op_context ctxt;
        return test(i, ctxt);
    }

    /**
     * Determines the whether the bit addressed by i is set or not. A context for
     * exploiting temporal locality can be provided.
     */
    bool test(index_type i, op_context& ctxt) const {
        value_t bit = (1ull << (i & LEAF_INDEX_MASK));
        return store.lookup(i >> LEAF_INDEX_WIDTH, ctxt) & bit;
    }

    /**
     * Determines the whether the bit addressed by i is set or not.
     */
    bool operator[](index_type i) const {
        return test(i);
    }

    /**
     * Resets all contained bits to 0.
     */
    void clear() {
        store.clear();
    }

    /**
     * Determines the number of bits set.
     */
    std::size_t size() const {
        // this is computed on demand to keep the set operation simple.
        std::size_t res = 0;
        for (const auto& cur : store) {
            res += __builtin_popcountll(cur.second);
        }
        return res;
    }

    /**
     * Computes the total memory usage of this data structure.
     */
    std::size_t getMemoryUsage() const {
        // compute the total memory usage
        return sizeof(*this) - sizeof(data_store_t) + store.getMemoryUsage();
    }

    /**
     * Sets all bits set in other to 1 within this bit map.
     */
    void addAll(const SparseBitMap& other) {
        // nothing to do if it is a self-assignment
        if (this == &other) return;

        // merge the sparse store
        store.addAll(other.store);
    }

    // ---------------------------------------------------------------------
    //                           Iterator
    // ---------------------------------------------------------------------

    /**
     * An iterator iterating over all indices set to 1.
     */
    class iterator : public std::iterator<std::forward_iterator_tag, index_type> {
        typedef typename data_store_t::iterator nested_iterator;

        // the iterator through the underlying sparse data structure
        nested_iterator iter;

        // the currently consumed mask
        uint64_t mask;

        // the value currently pointed to
        index_type value;

    public:
        // default constructor -- creating an end-iterator
        iterator() : mask(0) {}

        iterator(const nested_iterator& iter)
                : iter(iter), mask(toMask(iter->second)), value(iter->first << LEAF_INDEX_WIDTH) {
            moveToNextInMask();
        }

        iterator(const nested_iterator& iter, uint64_t m, index_type value)
                : iter(iter), mask(m), value(value) {}

        // a copy constructor
        iterator(const iterator& other) = default;

        // an assignment operator
        iterator& operator=(const iterator& other) = default;

        // the equality operator as required by the iterator concept
        bool operator==(const iterator& other) const {
            // only equivalent if pointing to the end
            return iter == other.iter && mask == other.mask;
        }

        // the not-equality operator as required by the iterator concept
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        // the deref operator as required by the iterator concept
        const index_type& operator*() const {
            return value;
        }

        // support for the pointer operator
        const index_type* operator->() const {
            return &value;
        }

        // the increment operator as required by the iterator concept
        iterator& operator++() {
            // progress in current mask
            if (moveToNextInMask()) return *this;

            // go to next entry
            ++iter;

            // update value
            if (!iter.isEnd()) {
                value = iter->first << LEAF_INDEX_WIDTH;
                mask = toMask(iter->second);
                moveToNextInMask();
            }

            // done
            return *this;
        }

        bool isEnd() const {
            return iter.isEnd();
        }

        void print(std::ostream& out) const {
            out << "SparseBitMapIter(" << iter << " -> " << std::bitset<64>(mask) << " @ " << value << ")";
        }

        // enables this iterator core to be printed (for debugging)
        friend std::ostream& operator<<(std::ostream& out, const iterator& iter) {
            iter.print(out);
            return out;
        }

        static uint64_t toMask(const value_t& value) {
            static_assert(sizeof(value_t) == sizeof(uint64_t), "Fixed for 64-bit compiler.");
            return reinterpret_cast<const uint64_t&>(value);
        }

    private:
        bool moveToNextInMask() {
            // check if there is something left
            if (mask == 0) return false;

            // get position of leading 1
            auto pos = __builtin_ctzll(mask);

            // consume this bit
            mask &= ~(1llu << pos);

            // update value
            value &= ~LEAF_INDEX_MASK;
            value |= pos;

            // done
            return true;
        }
    };

    /**
     * Obtains an iterator pointing to the first index set to 1. If there
     * is no such bit, end() will be returned.
     */
    iterator begin() const {
        auto it = store.begin();
        if (it.isEnd()) return end();
        return iterator(it);
    }

    /**
     * Returns an iterator referencing the position after the last set bit.
     */
    iterator end() const {
        return iterator();
    }

    /**
     * Obtains an iterator referencing the position i if the corresponding
     * bit is set, end() otherwise.
     */
    iterator find(index_type i) const {
        op_context ctxt;
        return find(i, ctxt);
    }

    /**
     * Obtains an iterator referencing the position i if the corresponding
     * bit is set, end() otherwise. An operation context can be provided
     * to exploit temporal locality.
     */
    iterator find(index_type i, op_context& ctxt) const {
        // check prefix part
        auto it = store.find(i >> LEAF_INDEX_WIDTH, ctxt);
        if (it.isEnd()) return end();

        // check bit-set part
        uint64_t mask = iterator::toMask(it->second);
        if (!(mask & (1llu << (i & LEAF_INDEX_MASK)))) return end();

        // OK, it is there => create iterator
        mask &= ((1ull << (i & LEAF_INDEX_MASK)) - 1);  // remove all bits before pos i
        return iterator(it, mask, i);
    }

    /**
     * A debugging utility printing the internal structure of this map to the
     * given output stream.
     */
    void dump(bool detail = false, std::ostream& out = std::cout) const {
        store.dump(detail, out);
    }

    /**
     * Provides write-protected access to the internal store for running
     * analysis on the data structure.
     */
    const data_store_t& getStore() const {
        return store;
    }
};

// ---------------------------------------------------------------------
//                              TRIE
// ---------------------------------------------------------------------

namespace detail {

/**
 * A base class for the Trie implementation allowing various
 * specializations of the Trie template to inherit common functionality.
 *
 * @tparam Dim the number of dimensions / arity of the stored tuples
 * @tparam Derived the type derived from this base class
 */
template <unsigned Dim, typename Derived>
class TrieBase {
public:
    /**
     * The type of the stored entries / tuples.
     */
    typedef typename ram::Tuple<RamDomain, Dim> entry_type;

    // -- operation wrappers --

    /**
     * A generic function enabling the insertion of tuple values in a user-friendly way.
     */
    template <typename... Values>
    bool insert(Values... values) {
        return static_cast<Derived&>(*this).insert((entry_type){{RamDomain(values)...}});
    }

    /**
     * A generic function enabling the convenient conduction of a membership check.
     */
    template <typename... Values>
    bool contains(Values... values) const {
        return static_cast<const Derived&>(*this).contains((entry_type){{RamDomain(values)...}});
    }

    // ---------------------------------------------------------------------
    //                           Iterator
    // ---------------------------------------------------------------------

    /**
     * An iterator over the stored entries.
     *
     * Iterators for tries consist of a top-level iterator maintaining the
     * master copy of a materialized tuple and a recursively nested iterator
     * core -- one for each nested trie level.
     */
    template <template <unsigned D> class IterCore>
    class iterator : public std::iterator<std::forward_iterator_tag, entry_type> {
        template <unsigned Len, unsigned Pos, unsigned Dimensions>
        friend struct fix_binding;

        template <unsigned Pos, unsigned Dimensions>
        friend struct fix_first;

        // the iterator core of this level
        typedef IterCore<0> iter_core_t;

        // the wrapped iterator
        iter_core_t iter_core;

        // the value currently pointed to
        entry_type value;

    public:
        // default constructor -- creating an end-iterator
        iterator() {}

        // a copy constructor
        iterator(const iterator& other) = default;

        iterator(iterator&& other) = default;

        template <typename Param>
        explicit iterator(const Param& param) : iter_core(param, value) {}

        // an assignment operator
        iterator& operator=(const iterator& other) = default;

        // the equality operator as required by the iterator concept
        bool operator==(const iterator& other) const {
            // equivalent if pointing to the same value
            return iter_core == other.iter_core;
        }

        // the not-equality operator as required by the iterator concept
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        // the deref operator as required by the iterator concept
        const entry_type& operator*() const {
            return value;
        }

        // support for the pointer operator
        const entry_type* operator->() const {
            return &value;
        }

        // the increment operator as required by the iterator concept
        iterator& operator++() {
            iter_core.inc(value);
            return *this;
        }

        // enables this iterator to be printed (for debugging)
        void print(std::ostream& out) const {
            out << "iter(" << iter_core << " -> " << value << ")";
        }

        friend std::ostream& operator<<(std::ostream& out, const iterator& iter) {
            iter.print(out);
            return out;
        }
    };
};

/**
 * A functor extracting a reference to a nested iterator core from an enclosing
 * iterator core.
 */
template <unsigned Level>
struct get_nested_iter_core {
    template <typename IterCore>
    auto operator()(IterCore& core) -> decltype(get_nested_iter_core<Level - 1>()(core.getNested())) {
        return get_nested_iter_core<Level - 1>()(core.getNested());
    }
};

template <>
struct get_nested_iter_core<0> {
    template <typename IterCore>
    IterCore& operator()(IterCore& core) {
        return core;
    }
};

/**
 * A functor initializing an iterator upon creation to reference the first
 * element in the associated Trie.
 */
template <unsigned Pos, unsigned Dim>
struct fix_first {
    template <unsigned bits, typename iterator>
    void operator()(const SparseBitMap<bits>& store, iterator& iter) const {
        // set iterator to first in store
        auto first = store.begin();
        get_nested_iter_core<Pos>()(iter.iter_core).setIterator(first);
    }

    template <typename Store, typename iterator>
    void operator()(const Store& store, iterator& iter) const {
        // set iterator to first in store
        auto first = store.begin();
        get_nested_iter_core<Pos>()(iter.iter_core).setIterator(first);
        // and continue recursively
        fix_first<Pos + 1, Dim>()(first->second->getStore(), iter);
    }
};

template <unsigned Dim>
struct fix_first<Dim, Dim> {
    template <typename Store, typename iterator>
    void operator()(const Store& store, iterator& iter) const {
        // terminal case => nothing to do
    }
};

/**
 * A functor initializing an iterator upon creation to reference the first element
 * exhibiting a given prefix within a given Trie.
 */
template <unsigned Len, unsigned Pos, unsigned Dim>
struct fix_binding {
    template <unsigned bits, typename iterator, typename entry_type>
    bool operator()(
            const SparseBitMap<bits>& store, iterator& begin, iterator& end, const entry_type& entry) const {
        // search in current level
        auto cur = store.find(entry[Pos]);

        // if not present => fail
        if (cur == store.end()) return false;

        // take current value
        get_nested_iter_core<Pos>()(begin.iter_core).setIterator(cur);
        ++cur;
        get_nested_iter_core<Pos>()(end.iter_core).setIterator(cur);

        // update iterator value
        begin.value[Pos] = entry[Pos];

        // no more remaining levels to fix
        return true;
    }

    template <typename Store, typename iterator, typename entry_type>
    bool operator()(const Store& store, iterator& begin, iterator& end, const entry_type& entry) const {
        // search in current level
        auto cur = store.find(entry[Pos]);

        // if not present => fail
        if (cur == store.end()) return false;

        // take current value as start
        get_nested_iter_core<Pos>()(begin.iter_core).setIterator(cur);

        // update iterator value
        begin.value[Pos] = entry[Pos];

        // fix remaining nested iterators
        auto res = fix_binding<Len - 1, Pos + 1, Dim>()(cur->second->getStore(), begin, end, entry);

        // update end of iterator
        if (get_nested_iter_core<Pos + 1>()(end.iter_core).getIterator() == cur->second->getStore().end()) {
            ++cur;
            if (cur != store.end()) {
                fix_first<Pos + 1, Dim>()(cur->second->getStore(), end);
            }
        }
        get_nested_iter_core<Pos>()(end.iter_core).setIterator(cur);

        // done
        return res;
    }
};

template <unsigned Pos, unsigned Dim>
struct fix_binding<0, Pos, Dim> {
    template <unsigned bits, typename iterator, typename entry_type>
    bool operator()(
            const SparseBitMap<bits>& store, iterator& begin, iterator& end, const entry_type& entry) const {
        // move begin to begin of store
        auto a = store.begin();
        get_nested_iter_core<Pos>()(begin.iter_core).setIterator(a);
        begin.value[Pos] = *a;

        return true;
    }

    template <typename Store, typename iterator, typename entry_type>
    bool operator()(const Store& store, iterator& begin, iterator& end, const entry_type& entry) const {
        // move begin to begin of store
        auto a = store.begin();
        get_nested_iter_core<Pos>()(begin.iter_core).setIterator(a);
        begin.value[Pos] = a->first;

        // continue recursively
        fix_binding<0, Pos + 1, Dim>()(a->second->getStore(), begin, end, entry);
        return true;
    }
};

template <unsigned Dim>
struct fix_binding<0, Dim, Dim> {
    template <typename Store, typename iterator, typename entry_type>
    bool operator()(const Store& store, iterator& begin, iterator& end, const entry_type& entry) const {
        // nothing more to do
        return true;
    }
};
}

/**
 * The most generic implementation of a Trie forming the top-level of any
 * Trie storing tuples of arity > 1.
 */
template <unsigned Dim>
class Trie : public detail::TrieBase<Dim, Trie<Dim>> {
    template <unsigned D>
    friend class Trie;

    template <unsigned D, typename Derived>
    friend class TrieBase;

    // a shortcut for the common base class type
    typedef typename detail::TrieBase<Dim, Trie<Dim>> base;

    // the type of the nested tries (1 dimension less)
    typedef Trie<Dim - 1> nested_trie_type;

    // the merge operation capable of merging two nested tries
    struct nested_trie_merger {
        nested_trie_type* operator()(nested_trie_type* a, const nested_trie_type* b) const {
            if (!b) return a;
            if (!a) return new nested_trie_type(*b);
            a->insertAll(*b);
            return a;
        }
    };

    // the operation capable of cloning a nested trie
    struct nested_trie_cloner {
        nested_trie_type* operator()(nested_trie_type* a) const {
            if (!a) return a;
            return new nested_trie_type(*a);
        }
    };

    // the data structure utilized for indexing nested tries
    typedef SparseArray<nested_trie_type*,
            6,  // = 2^6 entries per block
            nested_trie_merger, nested_trie_cloner>
            store_type;

    // the actual data store
    store_type store;

public:
    typedef typename ram::Tuple<RamDomain, Dim> entry_type;

    // ---------------------------------------------------------------------
    //                           Iterator
    // ---------------------------------------------------------------------

    /**
     * The iterator core for trie iterators involving this level.
     */
    template <unsigned I = 0>
    class iterator_core {
        // the iterator for the current level
        typedef typename store_type::iterator store_iter_t;

        // the type of the nested iterator
        typedef typename Trie<Dim - 1>::template iterator_core<I + 1> nested_iter_core;

        store_iter_t iter;

        nested_iter_core nested;

    public:
        /** default end-iterator constructor */
        iterator_core() {}

        template <typename Tuple>
        iterator_core(const store_iter_t& iter, Tuple& entry) : iter(iter) {
            entry[I] = iter->first;
            nested = iter->second->template getBeginCoreIterator<I + 1>(entry);
        }

        void setIterator(const store_iter_t& iter) {
            this->iter = iter;
        }

        store_iter_t& getIterator() {
            return this->iter;
        }

        nested_iter_core& getNested() {
            return nested;
        }

        template <typename Tuple>
        bool inc(Tuple& entry) {
            // increment nested iterator
            if (nested.inc(entry)) return true;

            // increment the iterator on this level
            ++iter;

            // check whether the end has been reached
            if (iter.isEnd()) return false;

            // otherwise update entry value
            entry[I] = iter->first;

            // and restart nested
            nested = iter->second->template getBeginCoreIterator<I + 1>(entry);
            return true;
        }

        bool operator==(const iterator_core& other) const {
            return nested == other.nested && iter == other.iter;
        }

        bool operator!=(const iterator_core& other) const {
            return !(*this == other);
        }

        // enables this iterator core to be printed (for debugging)
        void print(std::ostream& out) const {
            out << iter << " | " << nested;
        }

        friend std::ostream& operator<<(std::ostream& out, const iterator_core& iter) {
            iter.print(out);
            return out;
        }
    };

    // the type of iterator to be utilized when iterating of instances of this trie
    typedef typename base::template iterator<iterator_core> iterator;

    // the operation context aggregating all operation contexts of nested structures
    struct op_context {
        typedef typename store_type::op_context local_ctxt;
        typedef typename nested_trie_type::op_context nested_ctxt;

        // for insert and contain
        local_ctxt local;
        RamDomain lastQuery;
        nested_trie_type* lastNested;
        nested_ctxt nestedCtxt;

        // for boundaries
        unsigned lastBoundaryLevels;
        entry_type lastBoundaryRequest;
        range<iterator> lastBoundaries;

        op_context()
                : local(), lastNested(nullptr), lastBoundaryLevels(Dim + 1),
                  lastBoundaries(iterator(), iterator()) {}
    };

    using base::insert;
    using base::contains;

    /**
     * A simple destructore.
     */
    ~Trie() {
        for (auto& cur : store) {
            delete cur.second;  // clears all nested tries
        }
    }

    /**
     * Determines whether this trie is empty or not.
     */
    bool empty() const {
        return store.empty();
    }

    /**
     * Determines the number of entries in this trie.
     */
    std::size_t size() const {
        // the number of elements is lazy-evaluated
        std::size_t res = 0;
        for (const auto& cur : store) {
            res += cur.second->size();
        }
        return res;
    }

    /**
     * Computes the total memory usage of this data structure.
     */
    std::size_t getMemoryUsage() const {
        // compute the total memory usage of this level
        std::size_t res = sizeof(*this) - sizeof(store) + store.getMemoryUsage();

        // add the memory usage of sub-levels
        for (const auto& cur : store) {
            res += cur.second->getMemoryUsage();
        }

        // done
        return res;
    }

    /**
     * Removes all entries within this trie.
     */
    void clear() {
        // delete lower levels
        for (auto& cur : store) {
            delete cur.second;
        }

        // clear store
        store.clear();
    }

    /**
     * Inserts a new entry.
     *
     * @param tuple the entry to be added
     * @return true if the same tuple hasn't been present before, false otherwise
     */
    bool insert(const entry_type& tuple) {
        op_context ctxt;
        return insert(tuple, ctxt);
    }

    /**
     * Inserts a new entry. A operation context may be provided to exploit temporal
     * locality.
     *
     * @param tuple the entry to be added
     * @param ctxt the operation context to be utilized
     * @return true if the same tuple hasn't been present before, false otherwise
     */
    bool insert(const entry_type& tuple, op_context& ctxt) {
        return insert_internal<0>(tuple, ctxt);
    }

    /**
     * Determines whether a given tuple is present within the set specified
     * by this trie.
     *
     * @param tuple the tuple to be tested
     * @return true if present, false otherwise
     */
    bool contains(const entry_type& tuple) const {
        op_context ctxt;
        return contains(tuple, ctxt);
    }

    /**
     * Determines whether a given tuple is present within the set specified
     * by this trie. A operation context may be provided to exploit temporal
     * locality.
     *
     * @param tuple the entry to be added
     * @param ctxt the operation context to be utilized
     * @return true if the same tuple hasn't been present before, false otherwise
     */
    bool contains(const entry_type& tuple, op_context& ctxt) const {
        return contains_internal<0>(tuple, ctxt);
    }

    /**
     * Inserts all elements stored within the given trie into this trie.
     *
     * @param other the elements to be inserted into this trie
     */
    void insertAll(const Trie& other) {
        store.addAll(other.store);
    }

    /**
     * Obtains an iterator referencing the first element stored within this trie.
     */
    iterator begin() const {
        auto it = store.begin();
        if (it.isEnd()) return end();
        return iterator(it);
    }

    /**
     * Obtains an iterator referencing the position after the last element stored
     * within this trie.
     */
    iterator end() const {
        return iterator();
    }

    iterator find(const entry_type& entry) const {
        op_context ctxt;
        return find(entry, ctxt);
    }

    iterator find(const entry_type& entry, op_context& ctxt) const {
        auto range = getBoundaries<Dim>(entry, ctxt);
        return (!range.empty()) ? range.begin() : end();
    }

    /**
     * Obtains a range of elements matching the prefix of the given entry up to
     * levels elements.
     *
     * @tparam levels the length of the requested matching prefix
     * @param entry the entry to be looking for
     * @return the corresponding range of matching elements
     */
    template <unsigned levels>
    range<iterator> getBoundaries(const entry_type& entry) const {
        op_context ctxt;
        return getBoundaries<levels>(entry, ctxt);
    }

    /**
     * Obtains a range of elements matching the prefix of the given entry up to
     * levels elements. A operation context may be provided to exploit temporal
     * locality.
     *
     * @tparam levels the length of the requested matching prefix
     * @param entry the entry to be looking for
     * @param ctxt the operation context to be utilized
     * @return the corresponding range of matching elements
     */
    template <unsigned levels>
    range<iterator> getBoundaries(const entry_type& entry, op_context& ctxt) const {
        // if nothing is bound => just use begin and end
        if (levels == 0) return make_range(begin(), end());

        // check context
        if (ctxt.lastBoundaryLevels == levels) {
            bool fit = true;
            for (unsigned i = 0; i < levels; ++i) {
                fit = fit && (entry[i] == ctxt.lastBoundaryRequest[i]);
            }

            // if it fits => take it
            if (fit) {
                return ctxt.lastBoundaries;
            }
        }

        // start with two end iterators
        iterator begin, end;

        // adapt them level by level
        auto found = detail::fix_binding<levels, 0, Dim>()(store, begin, end, entry);
        if (!found) return make_range(iterator(), iterator());

        // update context
        ctxt.lastBoundaryLevels = levels;
        ctxt.lastBoundaryRequest = entry;
        ctxt.lastBoundaries = make_range(begin, end);

        // use the result
        return ctxt.lastBoundaries;
    }

    /**
     * Computes a partition of an approximate number of chunks of the content
     * of this trie. Thus, the union of the resulting set of disjoint ranges is
     * equivalent to the content of this trie.
     *
     * @param chunks the number of chunks requested
     * @return a list of sub-ranges forming a partition of the content of this trie
     */
    std::vector<range<iterator>> partition(unsigned chunks = 500) const {
        std::vector<range<iterator>> res;

        // shortcut for empty trie
        if (this->empty()) return res;

        // use top-level elements for partitioning
        int step = std::max(store.size() / chunks, size_t(1));

        int c = 1;
        auto priv = begin();
        for (auto it = store.begin(); it != store.end(); ++it, c++) {
            if (c % step != 0 || c == 1) continue;
            auto cur = iterator(it);
            res.push_back(make_range(priv, cur));
            priv = cur;
        }
        // add final chunk
        res.push_back(make_range(priv, end()));
        return res;
    }

    /**
     * Provides a protected access to the internally maintained store.
     */
    const store_type& getStore() const {
        return store;
    }

private:
    /**
     * Creates a core iterator for this trie level and updates component
     * I of the given entry to exhibit the corresponding first value.
     *
     * @tparam I the index of the tuple to be processed by the resulting iterator core
     * @tparam Tuple the type of the tuple to be processed by the resulting iterator core
     * @param entry a reference to the tuple to be updated to the first value
     * @return the requested iterator core instance
     */
    template <unsigned I, typename Tuple>
    iterator_core<I> getBeginCoreIterator(Tuple& entry) const {
        return iterator_core<I>(store.begin(), entry);
    }

    /**
     * The internally utilized implementation of the insert operation inserting
     * a given tuple into this sub-trie.
     *
     * @tparam I the component index associated to this level
     * @tparam Tuple the tuple type to be inserted
     * @param tuple the tuple to be inserted
     * @param ctxt a operation context to exploit temporal locality
     * @return true if this tuple wasn't contained before, false otherwise
     */
    template <unsigned I, typename Tuple>
    bool insert_internal(const Tuple& tuple, op_context& ctxt) {
        typedef typename store_type::value_type value_t;
        typedef typename store_type::atomic_value_type atomic_value_t;

        // check context
        if (ctxt.lastNested && ctxt.lastQuery == tuple[I]) {
            return ctxt.lastNested->template insert_internal<I + 1>(tuple, ctxt.nestedCtxt);
        }

        // lookup nested
        atomic_value_t& next = store.getAtomic(tuple[I], ctxt.local);

        // get pure pointer to next level
        value_t nextPtr = next;

        // conduct a lock-free lazy-creation of nested trees
        if (!nextPtr) {
            // create a new sub-tree
            auto newNested = new nested_trie_type();

            // register new sub-tree atomically
            if (next.compare_exchange_weak(nextPtr, newNested)) {
                nextPtr = newNested;  // worked
            } else {
                delete newNested;  // some other thread was faster => use its version
            }
        }

        // make sure a next has been established
        assert(nextPtr);

        // clear context if necessary
        if (nextPtr != ctxt.lastNested) {
            ctxt.lastQuery = tuple[I];
            ctxt.lastNested = nextPtr;
            ctxt.nestedCtxt = typename op_context::nested_ctxt();
        }

        // conduct recursive step
        return nextPtr->template insert_internal<I + 1>(tuple, ctxt.nestedCtxt);
    }

    /**
     * An internal implementation of the contains member function determining
     * whether a given tuple is present within this sub-trie or not.
     *
     * @tparam I the component index associated to this level
     * @tparam Tuple the tuple type to be checked
     * @param tuple the tuple to be checked
     * @param ctxt a operation context to exploit temporal locality
     * @return true if this tuple is present, false otherwise
     */
    template <unsigned I, typename Tuple>
    bool contains_internal(const Tuple& tuple, op_context& ctxt) const {
        // check context
        if (ctxt.lastNested && ctxt.lastQuery == tuple[I]) {
            return ctxt.lastNested->template contains_internal<I + 1>(tuple, ctxt.nestedCtxt);
        }

        // lookup next step
        auto next = store.lookup(tuple[I], ctxt.local);

        // clear context if necessary
        if (next != ctxt.lastNested) {
            ctxt.lastQuery = tuple[I];
            ctxt.lastNested = next;
            ctxt.nestedCtxt = typename op_context::nested_ctxt();
        }

        // conduct recursive step
        return next && next->template contains_internal<I + 1>(tuple, ctxt.nestedCtxt);
    }
};

/**
 * A template specialization for the 0-ary try.
 */
template <>
class Trie<0u> : public detail::TrieBase<0u, Trie<0u>> {
    template <unsigned Dim>
    friend class Trie;

    template <unsigned Dim, typename Derived>
    friend class detail::TrieBase;

    typedef typename detail::TrieBase<0u, Trie<0u>> base;

    typedef typename ram::Tuple<RamDomain, 0> entry_type;

    // the singleton instance of the 0-ary tuple
    static const ram::Tuple<RamDomain, 0> instance;

    // a flag determining whether this trie is empty or contains the singleton instance
    bool present;

public:
    struct op_context {};

    using base::insert;
    using base::contains;

    // a simple default constructor
    Trie() : present(false) {}

    /**
     * Clears the content of this trie.
     */
    void clear() {
        present = false;
    }

    /**
     * Determines whether this trie is empty or not.
     */
    bool empty() const {
        return !present;
    }

    /**
     * Determines the number of elements stores in this trie.
     */
    std::size_t size() const {
        return (present) ? 1 : 0;
    }

    /**
     * Computes the total memory usage of this data structure.
     */
    std::size_t getMemoryUsage() const {
        // compute the total memory usage
        return sizeof(*this);
    }

    /**
     * Inserts a new element into this trie.
     *
     * @return true if the trie has been empty before, false otherwise
     */
    bool insert(const entry_type&) {
        bool res = !present;
        present = true;
        return res;
    }

    /**
     * Inserts a new element into this trie. An operation context may
     * be provided, but will be ignored.
     *
     * @return true if the trie has been empty before, false otherwise
     */
    bool insert(const entry_type& tuple, op_context&) {
        return insert(tuple);
    }

    /**
     * Adds all elements of the given trie to this trie.
     */
    void insertAll(const Trie& other) {
        present = present || other.present;
    }

    /**
     * Determines whether the given 0-ary tuple is present within this trie.
     */
    bool contains(const entry_type&) const {
        return present;
    }

    /**
     * Determines whether the given 0-ary tuple is present within this trie.
     * An operation context may be provided, but ignored.
     */
    bool contains(const entry_type& tuple, op_context&) const {
        return contains(tuple);
    }

    // ---------------------------------------------------------------------
    //                           Iterator
    // ---------------------------------------------------------------------

    /**
     * The iterator core for this type of trie level.
     */
    template <unsigned I = 0>
    class iterator_core {
        bool end;

    public:
        iterator_core(bool end = true) : end(end) {}

        template <typename Tuple>
        iterator_core(bool end, Tuple&) : end(end) {}

        bool inc() {
            auto res = end;
            end = true;
            return res;
        }

        template <typename Tuple>
        bool inc(Tuple&) {
            return inc();
        }

        bool operator==(const iterator_core& other) const {
            return end && other.end;
        }

        bool operator!=(const iterator_core& other) const {
            return !(*this == other);
        }

        // enables this iterator core to be printed (for debugging)
        void print(std::ostream& out) const {
            out << ((end) ? "end" : "single");
        }

        friend std::ostream& operator<<(std::ostream& out, const iterator_core& iter) {
            iter.print(out);
            return out;
        }
    };

    typedef typename base::template iterator<iterator_core> iterator;

    /**
     * Obtains an iterator referencing the first element in this trie or
     * end() if this trie is empty.
     */
    iterator begin() const {
        return iterator(!present);
    }

    /**
     * Obtains an iterator positions after the last element stored in this
     * trie.
     */
    iterator end() const {
        return iterator();
    }

    /**
     * Partitions this trie into a list of disjoint sub-sets.
     */
    std::vector<range<iterator>> partition(unsigned chunks = 500) const {
        // shortcut for empty trie
        if (empty()) return std::vector<range<iterator>>();
        return toVector(make_range(begin(), end()));
    }

    /**
     * Obtains boundaries for the given entry. Since tuples stored
     * in this trie have no components, the result will always be the
     * full range [begin(),...,end()]. Levels != 0 are not allowed.
     */
    template <unsigned levels>
    range<iterator> getBoundaries(const entry_type&) const {
        static_assert(levels == 0, "No dimensions to query!");
        return make_range(begin(), end());
    }

    /**
     * Obtains boundaries for the given entry. Since tuples stored
     * in this trie have no components, the result will always be the
     * full range [begin(),...,end()]. Levels != 0 are not allowed.
     */
    template <unsigned levels>
    range<iterator> getBoundaries(const entry_type& entry, op_context&) const {
        return getBoundaries<levels>(entry);
    }

private:
    /**
     * Initialized the core iterator of this level.
     */
    template <unsigned I, typename Tuple>
    iterator_core<I> getBeginCoreIterator(Tuple&) const {
        return iterator_core<I>(!present);
    }

    /**
     * The internal implementation of the insert operation on this level
     * as it is utilized by the TrieBase class.
     */
    template <unsigned I, typename Tuple>
    bool insert_internal(const Tuple& tuple, op_context&) {
        return insert(tuple);
    }

    /**
     * The internal implementation of the contains operation on this level
     * as it is utilized by the TrieBase class.
     */
    template <unsigned I, typename Tuple>
    bool contains_internal(const Tuple&, op_context&) const {
        return present;
    }
};

/**
 * A template specialization for tries containing tuples exhibiting a single
 * element. For improved memory efficiency, this level is the leaf-node level
 * of all tires exhibiting an arity >= 1. Internally, values are stored utilizing
 * sparse bit maps.
 */
template <>
class Trie<1u> : public detail::TrieBase<1u, Trie<1u>> {
    template <unsigned Dim>
    friend class Trie;

    template <unsigned Dim, typename Derived>
    friend class detail::TrieBase;

    // a shortcut for the base type
    typedef typename detail::TrieBase<1u, Trie<1u>> base;

    // the map type utilized internally
    typedef SparseBitMap<> map_type;

    // the internal data store
    map_type map;

public:
    typedef typename map_type::op_context op_context;

    using base::insert;
    using base::contains;

    /**
     * Determines whether this trie is empty or not.
     */
    bool empty() const {
        return map.empty();
    }

    /**
     * Determines the number of elements stored in this trie.
     */
    std::size_t size() const {
        return map.size();
    }

    /**
     * Computes the total memory usage of this data structure.
     */
    std::size_t getMemoryUsage() const {
        // compute the total memory usage
        return sizeof(*this) - sizeof(map_type) + map.getMemoryUsage();
    }

    /**
     * Removes all elements form this trie.
     */
    void clear() {
        map.clear();
    }

    /**
     * Inserts the given tuple into this trie.
     *
     * @param tuple the tuple to be inserted
     * @return true if the tuple has not been present before, false otherwise
     */
    bool insert(const entry_type& tuple) {
        op_context ctxt;
        return insert(tuple, ctxt);
    }

    /**
     * Inserts the given tuple into this trie.
     * An operation context can be provided to exploit temporal locality.
     *
     * @param tuple the tuple to be inserted
     * @param ctxt an operation context for exploiting temporal locality
     * @return true if the tuple has not been present before, false otherwise
     */
    bool insert(const entry_type& tuple, op_context& ctxt) {
        return insert_internal<0>(tuple, ctxt);
    }

    /**
     * Determines whether the given tuple is present in this trie or not.
     *
     * @param tuple the tuple to be tested
     * @return true if present, false otherwise
     */
    bool contains(const entry_type& tuple) const {
        op_context ctxt;
        return contains(tuple, ctxt);
    }

    /**
     * Determines whether the given tuple is present in this trie or not.
     * An operation context can be provided to exploit temporal locality.
     *
     * @param tuple the tuple to be tested
     * @param ctxt an operation context for exploiting temporal locality
     * @return true if present, false otherwise
     */
    bool contains(const entry_type& tuple, op_context& ctxt) const {
        return contains_internal<0>(tuple, ctxt);
    }

    /**
     * Inserts all tuples stored within the given trie into this trie.
     * This operation is considerably more efficient than the consecutive
     * insertion of the elements in other into this trie.
     */
    void insertAll(const Trie& other) {
        map.addAll(other.map);
    }

    // ---------------------------------------------------------------------
    //                           Iterator
    // ---------------------------------------------------------------------

    /**
     * The iterator core of this level contributing to the construction of
     * a composed trie iterator.
     */
    template <unsigned I = 0>
    class iterator_core {
        // the iterator for this level
        typedef typename map_type::iterator iter_type;

        // the referenced bit-map iterator
        iter_type iter;

    public:
        /** default end-iterator constructor */
        iterator_core() {}

        template <typename Tuple>
        iterator_core(const iter_type& iter, Tuple& entry) : iter(iter) {
            entry[I] = *iter;
        }

        void setIterator(const iter_type& iter) {
            this->iter = iter;
        }

        iter_type& getIterator() {
            return this->iter;
        }

        template <typename Tuple>
        bool inc(Tuple& entry) {
            // increment the iterator on this level
            ++iter;

            // check whether the end has been reached
            if (iter.isEnd()) return false;

            // otherwise update entry value
            entry[I] = *iter;
            return true;
        }

        bool operator==(const iterator_core& other) const {
            return iter == other.iter;
        }

        bool operator!=(const iterator_core& other) const {
            return !(*this == other);
        }

        // enables this iterator core to be printed (for debugging)
        void print(std::ostream& out) const {
            out << iter;
        }

        friend std::ostream& operator<<(std::ostream& out, const iterator_core& iter) {
            iter.print(out);
            return out;
        }
    };

    // the iterator type utilized by this trie type
    typedef typename base::template iterator<iterator_core> iterator;

    /**
     * Obtains an iterator referencing the first element stored within this trie
     * or end() if this trie is empty.
     */
    iterator begin() const {
        if (map.empty()) return end();
        return iterator(map.begin());
    }

    /**
     * Obtains an iterator referencing the first position after the last element
     * within this trie.
     */
    iterator end() const {
        return iterator();
    }

    /**
     * Obtains a partition of this tire such that the resulting list of ranges
     * cover disjoint subsets of the elements stored in this trie. Their union
     * is equivalent to the content of this trie.
     */
    std::vector<range<iterator>> partition(unsigned chunks = 500) const {
        std::vector<range<iterator>> res;

        // shortcut for empty trie
        if (this->empty()) return res;

        // use top-level elements for partitioning
        int step = std::max(map.size() / chunks, size_t(1));

        int c = 1;
        auto priv = begin();
        for (auto it = map.begin(); it != map.end(); ++it, c++) {
            if (c % step != 0 || c == 1) continue;
            auto cur = iterator(it);
            res.push_back(make_range(priv, cur));
            priv = cur;
        }
        // add final chunk
        res.push_back(make_range(priv, end()));
        return res;
    }

    /**
     * Obtains a range of elements matching the prefix of the given entry up to
     * levels elements.
     *
     * @tparam levels the length of the requested matching prefix
     * @param entry the entry to be looking for
     * @return the corresponding range of matching elements
     */
    template <unsigned levels>
    range<iterator> getBoundaries(const entry_type& entry) const {
        op_context ctxt;
        return getBoundaries<levels>(entry, ctxt);
    }

    /**
     * Obtains a range of elements matching the prefix of the given entry up to
     * levels elements. A operation context may be provided to exploit temporal
     * locality.
     *
     * @tparam levels the length of the requested matching prefix
     * @param entry the entry to be looking for
     * @param ctxt the operation context to be utilized
     * @return the corresponding range of matching elements
     */
    template <unsigned levels>
    range<iterator> getBoundaries(const entry_type& entry, op_context& ctxt) const {
        // for levels = 0
        if (levels == 0) return make_range(begin(), end());
        // for levels = 1
        auto pos = map.find(entry[0], ctxt);
        if (pos == map.end()) return make_range(end(), end());
        auto next = pos;
        ++next;
        return make_range(iterator(pos), iterator(next));
    }

    /**
     * Provides protected access to the internally maintained store.
     */
    const map_type& getStore() const {
        return map;
    }

private:
    /**
     * Creates a core iterator for this trie level and updates component
     * I of the given entry to exhibit the corresponding first value.
     *
     * @tparam I the index of the tuple to be processed by the resulting iterator core
     * @tparam Tuple the type of the tuple to be processed by the resulting iterator core
     * @param entry a reference to the tuple to be updated to the first value
     * @return the requested iterator core instance
     */
    template <unsigned I, typename Tuple>
    iterator_core<I> getBeginCoreIterator(Tuple& entry) const {
        return iterator_core<I>(map.begin(), entry);
    }

    /**
     * The internally utilized implementation of the insert operation inserting
     * a given tuple into this sub-trie.
     *
     * @tparam I the component index associated to this level
     * @tparam Tuple the tuple type to be inserted
     * @param tuple the tuple to be inserted
     * @param ctxt a operation context to exploit temporal locality
     * @return true if this tuple wasn't contained before, false otherwise
     */
    template <unsigned I, typename Tuple>
    bool insert_internal(const Tuple& tuple, op_context& ctxt) {
        return map.set(tuple[I], ctxt);
    }

    /**
     * An internal implementation of the contains member function determining
     * whether a given tuple is present within this sub-trie or not.
     *
     * @tparam I the component index associated to this level
     * @tparam Tuple the tuple type to be checked
     * @param tuple the tuple to be checked
     * @param ctxt a operation context to exploit temporal locality
     * @return true if this tuple is present, false otherwise
     */
    template <unsigned I, typename Tuple>
    bool contains_internal(const Tuple& tuple, op_context& ctxt) const {
        return map.test(tuple[I], ctxt);
    }
};

}  // end namespace souffle
