/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledRamIndexUtils.h
 *
 * The central file covering the data structure utilized by
 * the souffle compiler for representing relations in compiled queries.
 *
 ***********************************************************************/

#pragma once

#include "BTree.h"
#include "BinaryRelation.h"
#include "CompiledRamTuple.h"
#include "IterUtils.h"
#include "ParallelUtils.h"
#include "SymbolTable.h"
#include "Table.h"
#include "Trie.h"
#include "Util.h"

#include <iterator>
#include <mutex>
#include <type_traits>

#include <libgen.h>

namespace souffle {

namespace ram {

/**
 * A namespace enclosing template-meta-programming utilities for handling
 * parameter lists for templates.
 */
namespace column_utils {

// -- checks whether a given number is contained in a list of numbers --

template <unsigned E, unsigned... List>
struct contains;

template <unsigned E>
struct contains<E> {
    enum { value = false };
};

template <unsigned E, unsigned F, unsigned... Rest>
struct contains<E, F, Rest...> {
    enum { value = (E == F) || contains<E, Rest...>::value };
};

// -- check uniqueness of a list of integer values --

template <unsigned... List>
struct unique;

template <>
struct unique<> {
    enum { value = true };
};

template <unsigned E, unsigned... Rest>
struct unique<E, Rest...> {
    enum { value = !contains<E, Rest...>::value && unique<Rest...>::value };
};

}  // end namespace column_utils

/**
 * A namespace enclosing utilities required by indices.
 */
namespace index_utils {

// -------- generic tuple comparator ----------

template <unsigned... Columns>
struct comparator;

template <unsigned First, unsigned... Rest>
struct comparator<First, Rest...> {
    template <typename T>
    int operator()(const T& a, const T& b) const {
        return (a[First] < b[First]) ? -1 : ((a[First] > b[First]) ? 1 : comparator<Rest...>()(a, b));
    }
    template <typename T>
    bool less(const T& a, const T& b) const {
        return a[First] < b[First] || (a[First] == b[First] && comparator<Rest...>().less(a, b));
    }
    template <typename T>
    bool equal(const T& a, const T& b) const {
        return a[First] == b[First] && comparator<Rest...>().equal(a, b);
    }
};

template <>
struct comparator<> {
    template <typename T>
    int operator()(const T& a, const T& b) const {
        return 0;
    }
    template <typename T>
    bool less(const T& a, const T& b) const {
        return false;
    }
    template <typename T>
    bool equal(const T& a, const T& b) const {
        return true;
    }
};

// ----- a comparator wrapper dereferencing pointers ----------
//         (required for handling indirect indices)

template <typename Comp>
struct deref_compare {
    template <typename T>
    int operator()(const T& a, const T& b) const {
        Comp comp;
        return comp(*a, *b);
    }
    template <typename T>
    bool less(const T& a, const T& b) const {
        Comp comp;
        return comp.less(*a, *b);
    }
    template <typename T>
    bool equal(const T& a, const T& b) const {
        Comp comp;
        return comp.equal(*a, *b);
    }
};

// ----- a utility for printing lists of parameters -------
//    (required for printing descriptions of relations)

template <unsigned... Columns>
struct print;

template <>
struct print<> {
    friend std::ostream& operator<<(std::ostream& out, const print&) {
        return out;
    }
};

template <unsigned Last>
struct print<Last> {
    friend std::ostream& operator<<(std::ostream& out, const print&) {
        return out << Last;
    }
};

template <unsigned First, unsigned Second, unsigned... Rest>
struct print<First, Second, Rest...> {
    friend std::ostream& operator<<(std::ostream& out, const print&) {
        return out << First << "," << print<Second, Rest...>();
    }
};

}  // end namespace utils

/**
 * The index class is utilized as a template-meta-programming structure
 * to specify and realize indices.
 *
 * @tparam Columns ... the order in which elements of the relation to be indexed
 * 				shell be considered by this index.
 */
template <unsigned... Columns>
struct index {
    // check validity of this index - column fields need to be unique
    static_assert(column_utils::unique<Columns...>::value, "Invalid duplication of columns!");

    // the comparator associated to this index
    typedef index_utils::comparator<Columns...> comparator;

    // enables to check whether the given column is covered by this index or not
    template <unsigned Col>
    struct covers {
        enum { value = column_utils::contains<Col, Columns...>::value };
    };

    // the length of the index
    enum { size = sizeof...(Columns) };

    // enables instances of this class to be printed (for printing relation-structure descriptions)
    friend std::ostream& operator<<(std::ostream& out, const index& index) {
        return out << "<" << index_utils::print<Columns...>() << ">";
    }
};

/**
 * A namespace enclosing utilities required relations to handle indices.
 */
namespace index_utils {

// -------------------------------------------------------------
//                     Static Index Utilities
// -------------------------------------------------------------

// -- check whether a given list only consists of indices --

template <typename... Index>
struct all_indices {
    enum { value = false };
};

template <>
struct all_indices<> {
    enum { value = true };
};

template <unsigned... Columns, typename... Rest>
struct all_indices<index<Columns...>, Rest...> {
    enum { value = all_indices<Rest...>::value };
};

// -- checks whether a list of typed contains a certain type --

template <typename E, typename... List>
struct contains;

template <typename E>
struct contains<E> {
    enum { value = false };
};

template <typename E, typename F, typename... Rest>
struct contains<E, F, Rest...> {
    enum { value = contains<E, Rest...>::value };
};

template <typename E, typename... Rest>
struct contains<E, E, Rest...> {
    enum { value = true };
};

// -- check whether a given list is a list of unique indices --

template <typename... Index>
struct unique;

template <>
struct unique<> {
    enum { value = true };
};

template <typename First, typename... Rest>
struct unique<First, Rest...> {
    enum { value = all_indices<First, Rest...>::value && !contains<First, Rest...>::value };
};

// -- check whether the columns of an index are not exceeding a given arity --

template <unsigned arity, typename Index>
struct check_index_arity {
    enum { value = false };
};

template <unsigned arity>
struct check_index_arity<arity, index<>> {
    enum { value = true };
};

template <unsigned arity, unsigned F, unsigned... Rest>
struct check_index_arity<arity, index<F, Rest...>> {
    enum { value = (F < arity) && check_index_arity<arity, index<Rest...>>::value };
};

// -- check whether the columns of a list of indices are not exceeding a given arity --

template <unsigned arity, typename... Index>
struct check_arity;

template <unsigned arity>
struct check_arity<arity> {
    enum { value = true };
};

template <unsigned arity, typename F, typename... Rest>
struct check_arity<arity, F, Rest...> {
    enum { value = check_index_arity<arity, F>::value && check_arity<arity, Rest...>::value };
};

// -- checks the validity of a given list of indices --

template <unsigned arity, typename... Indices>
struct check {
    enum {
        value = unique<Indices...>::value &&           // indices need to be unique
                check_arity<arity, Indices...>::value  // and valid
    };
};

// -- a utility extending a given index by another column --
//   e.g. index<1,0>   =>    index<1,0,2>

template <typename Index, unsigned column>
struct extend;

template <unsigned... Columns, unsigned Col>
struct extend<index<Columns...>, Col> {
    typedef index<Columns..., Col> type;
};

// -- a utility concatenating indices --

template <typename I1, typename I2>
struct concat;

template <unsigned... C1, unsigned... C2>
struct concat<index<C1...>, index<C2...>> {
    typedef index<C1..., C2...> type;
};

// -- obtains a full index for a given arity --

template <unsigned arity>
struct get_full_index {
    typedef typename extend<typename get_full_index<arity - 1>::type, arity - 1>::type type;
};

template <>
struct get_full_index<0> {
    typedef index<> type;
};

// -- extends a given index to a full index --

namespace detail {

// an auxiliary type required to implement index extension
template <unsigned i, unsigned arity, typename Index>
struct extend_to_full_index_aux {
    typedef typename extend_to_full_index_aux<i + 1, arity,
            typename std::conditional<(Index::template covers<i>::value), Index,
                                                      typename extend<Index, i>::type>::type>::type type;
};

template <unsigned arity, typename Index>
struct extend_to_full_index_aux<arity, arity, Index> {
    typedef Index type;
};
}

template <unsigned arity, typename Index>
struct extend_to_full_index : public detail::extend_to_full_index_aux<0, arity, Index> {};

// -- checks whether one index is a prefix of another index --

template <typename I1, typename I2>
struct is_prefix {
    enum { value = false };
};

template <unsigned... Rest>
struct is_prefix<index<>, index<Rest...>> {
    enum { value = true };
};

template <unsigned F, unsigned... Ra, unsigned... Rb>
struct is_prefix<index<F, Ra...>, index<F, Rb...>> {
    enum { value = is_prefix<index<Ra...>, index<Rb...>>::value };
};

// -- obtains the prefix of an index --

namespace detail {

template <unsigned L, unsigned... Rest>
struct get_prefix_aux;

template <unsigned L, unsigned First, unsigned... Rest>
struct get_prefix_aux<L, First, Rest...> {
    typedef typename concat<index<First>, typename get_prefix_aux<L - 1, Rest...>::type>::type type;
};

template <unsigned First, unsigned... Rest>
struct get_prefix_aux<0, First, Rest...> {
    typedef index<> type;
};

template <>
struct get_prefix_aux<0> {
    typedef index<> type;
};
}

template <unsigned L, typename I>
struct get_prefix;

template <unsigned L, unsigned... Rest>
struct get_prefix<L, index<Rest...>> {
    typedef typename detail::get_prefix_aux<L, Rest...>::type type;
};

// -- determines whether the columns of one index is a subset of the columns of another index --

template <typename I1, typename I2>
struct is_subset_of {
    enum { value = false };
};

template <unsigned First, unsigned... Rest, unsigned... Full>
struct is_subset_of<index<First, Rest...>, index<Full...>> {
    enum {
        value = column_utils::contains<First, Full...>::value &&
                is_subset_of<index<Rest...>, index<Full...>>::value
    };
};

template <unsigned... Full>
struct is_subset_of<index<>, index<Full...>> {
    enum { value = true };
};

// -- checks whether one index is a permutation of another index --

template <typename I1, typename I2>
struct is_permutation {
    enum { value = false };
};

template <unsigned... C1, unsigned... C2>
struct is_permutation<index<C1...>, index<C2...>> {
    enum { value = sizeof...(C1) == sizeof...(C2) && is_subset_of<index<C1...>, index<C2...>>::value };
};

// -- checks whether one index is an extension of another index --

namespace detail {

template <typename P1, typename R1, typename P2, typename R2>
struct is_compatible_with_aux {
    enum { value = false };
};

template <unsigned... A1, unsigned A, unsigned... A2, unsigned... B1, unsigned B, unsigned... B2>
struct is_compatible_with_aux<index<A1...>, index<A, A2...>, index<B1...>, index<B, B2...>> {
    enum {
        value = is_compatible_with_aux<index<A1..., A>, index<A2...>, index<B1..., B>, index<B2...>>::value
    };
};

template <unsigned... A, unsigned... B, unsigned... R>
struct is_compatible_with_aux<index<A...>, index<>, index<B...>, index<R...>> {
    enum { value = is_permutation<index<A...>, index<B...>>::value };
};
}

template <typename I1, typename I2>
struct is_compatible_with {
    enum { value = false };
};

template <unsigned... C1, unsigned... C2>
struct is_compatible_with<index<C1...>, index<C2...>> {
    enum { value = detail::is_compatible_with_aux<index<>, index<C1...>, index<>, index<C2...>>::value };
};

// -- check whether there is a full index in a list of indices --

template <unsigned arity, typename... Indices>
struct contains_full_index;

template <unsigned arity, typename First, typename... Rest>
struct contains_full_index<arity, First, Rest...> {
    enum { value = First::size == arity || contains_full_index<arity, Rest...>::value };
};

template <unsigned arity>
struct contains_full_index<arity> {
    enum { value = false };
};

// -- get first full index from a list of indices --

template <unsigned arity, typename... Indices>
struct get_first_full_index;

template <unsigned arity, typename First, typename... Rest>
struct get_first_full_index<arity, First, Rest...> {
    typedef typename std::conditional<First::size == arity, First,
            typename get_first_full_index<arity, Rest...>::type>::type type;
};

template <unsigned arity, typename Index>
struct get_first_full_index<arity, Index> {
    typedef Index type;
};

// -------------------------------------------------------------
//                   Tuple Masking Utils
// -------------------------------------------------------------

/**
 * A utility masking out all the tuple columns covered by
 * index From but not by index To using the given value.
 *
 * @tparam From  .. the initial index
 * @tparam To    .. the target index
 * @tparam value .. the value to be assigned to the filed
 *          covered by From but not by To
 */
template <typename From, typename To, RamDomain value>
struct mask;

// masks out uncovered columns
template <unsigned F, unsigned... R, unsigned... M, RamDomain value>
struct mask<index<F, R...>, index<M...>, value> {
    template <typename T>
    void operator()(T& t) const {
        if (!column_utils::contains<F, M...>::value) t[F] = value;
        mask<index<R...>, index<M...>, value>()(t);
    }
};

// the terminal case
template <unsigned... M, RamDomain value>
struct mask<index<>, index<M...>, value> {
    template <typename T>
    void operator()(T& t) const {}
};

/**
 * Sets all fields in the given tuple that are covered by From but
 * not by the index To to the minimal value.
 *
 * @tparam From .. the initial index
 * @tparam To   .. the target index
 * @tparam T    .. the tuple type
 * @param tuple .. the tuple to be processed
 * @return the given tuple with masked out fields
 */
template <typename From, typename To, typename T>
T lower(T tuple) {
    mask<From, To, MIN_RAM_DOMAIN>()(tuple);
    return tuple;
}

/**
 * Sets all fields in the given tuple that are covered by From but
 * not by the index To to the maximal value.
 *
 * @tparam From .. the initial index
 * @tparam To   .. the target index
 * @tparam T    .. the tuple type
 * @param tuple .. the tuple to be processed
 * @return the given tuple with masked out fields
 */
template <typename From, typename To, typename T>
T raise(T tuple) {
    mask<From, To, MAX_RAM_DOMAIN>()(tuple);
    return tuple;
}

// -------------------------------------------------------------
// 					     Index Container
// -------------------------------------------------------------

/**
 * A direct index storing the indexed elements directly within the maintained
 * index structure.
 *
 * @tparam Tuple .. the type of tuple to be maintained by this index
 * @tparam Index .. the index to be internally utilized.
 */
template <typename Tuple, typename Index>
struct DirectIndex {
    typedef btree_set<Tuple, typename Index::comparator> data_structure;

    typedef typename data_structure::key_type key_type;

    typedef typename data_structure::const_iterator iterator;

    typedef typename data_structure::operation_hints operation_hints;

private:
    data_structure index;

public:
    bool empty() const {
        return index.empty();
    }

    std::size_t size() const {
        return index.size();
    }

    bool insert(const key_type& key, operation_hints& hints) {
        // insert the element (insert is synchronized internally)
        return index.insert(key, hints);
    }

    void insertAll(const DirectIndex& other) {
        // use index's insert-all
        index.insertAll(other.index);
    }

    bool contains(const key_type& key, operation_hints& hints) const {
        return index.contains(key, hints);
    }

    iterator find(const key_type& key, operation_hints& hints) const {
        return index.find(key, hints);
    }

    template <typename SubIndex>
    range<iterator> equalRange(const key_type& key, operation_hints& hints) const {
        // more efficient support for full-indices
        if (int(SubIndex::size) == int(Index::size) && int(Index::size) == int(key_type::arity)) {
            // in this case there is at most one element with this value
            auto pos = find(key, hints);
            auto end = index.end();
            if (pos != end) {
                end = pos;
                ++end;
            }
            return make_range(pos, end);
        }

        // compute lower and upper bounds
        return make_range(index.lower_bound(lower<Index, SubIndex>(key), hints),
                index.upper_bound(raise<Index, SubIndex>(key), hints));
    }

    iterator begin() const {
        return index.begin();
    }

    iterator end() const {
        return index.end();
    }

    void clear() {
        index.clear();
    }

    std::vector<range<iterator>> partition() const {
        return index.getChunks(400);
    }

    static void printDescription(std::ostream& out) {
        out << "direct-btree-index(" << Index() << ")";
    }
};

/**
 * A wrapper realizing indirect indices -- by storing pointers to the
 * actually indexed values.
 *
 * @tparam Tuple .. the type of tuple to be maintained by this index
 * @tparam Index .. the index to be internally utilized.
 */
template <typename Tuple, typename Index>
struct IndirectIndex {
    typedef typename std::conditional<(int)(Tuple::arity) == (int)(Index::size),
            btree_set<const Tuple*, index_utils::deref_compare<typename Index::comparator>>,
            btree_multiset<const Tuple*, index_utils::deref_compare<typename Index::comparator>>>::type
            data_structure;

    typedef typename std::remove_cv<
            typename std::remove_pointer<typename data_structure::key_type>::type>::type key_type;

    typedef IterDerefWrapper<typename data_structure::const_iterator> iterator;

    typedef typename data_structure::operation_hints operation_hints;

private:
    // the enclosed index
    data_structure index;

public:
    bool empty() const {
        return index.empty();
    }

    std::size_t size() const {
        return index.size();
    }

    bool insert(const key_type& key, operation_hints& hints) {
        // insert the element (insert is synchronized internally)
        return index.insert(&key, hints);
    }

    void insertAll(const IndirectIndex& other) {
        // use index's insert-all
        index.insertAll(other.index);
    }

    bool contains(const key_type& key, operation_hints& hints) const {
        return index.contains(&key, hints);
    }

    iterator find(const key_type& key, operation_hints& hints) const {
        return index.find(&key, hints);
    }

    template <typename SubIndex>
    range<iterator> equalRange(const key_type& key, operation_hints& hints) const {
        // more efficient support for full-indices
        if (int(SubIndex::size) == int(Index::size) && int(Index::size) == int(key_type::arity)) {
            // in this case there is at most one element with this value
            auto pos = find(key, hints);
            auto end = this->end();
            if (pos != end) {
                end = pos;
                ++end;
            }
            return make_range(pos, end);
        }

        // compute lower and upper bounds
        auto low = lower<Index, SubIndex>(key);
        auto hig = raise<Index, SubIndex>(key);
        return make_range(
                derefIter(index.lower_bound(&low, hints)), derefIter(index.upper_bound(&hig, hints)));
    }

    iterator begin() const {
        return index.begin();
    }

    iterator end() const {
        return index.end();
    }

    void clear() {
        index.clear();
    }

    /**
     * Return a list of iterators, s.t. we can process in parallel.
     * "an approximation of the number of sub-ranges to be included in the resulting partition."
     * "The numbers there [400] are not very important, as long as they are large enough (~10x the number of
     * maximum expected cores)
     * and not too large, such that processing a fragment lasts less than e.g. 1ms."
     * @param np the number of iterators to attempt to return, which represents slices of the data structure
     * @return a vector of these iterator ranges
     */
    std::vector<range<iterator>> partition() const {
        std::vector<range<iterator>> res;
        for (const auto& cur : index.getChunks(400)) {
            res.push_back(make_range(derefIter(cur.begin()), derefIter(cur.end())));
        }
        return res;
    }

    static void printDescription(std::ostream& out) {
        out << "indirect-btree-index(" << Index() << ")";
    }
};

// -------------------------------------------------------------

template <unsigned Pos, unsigned... Order>
struct aux_order;

template <unsigned Pos, unsigned First, unsigned... Rest>
struct aux_order<Pos, First, Rest...> {
    template <typename tuple_type>
    void order_in(tuple_type& res, const tuple_type& in) const {
        res[Pos] = in[First];
        aux_order<Pos + 1, Rest...>().order_in(res, in);
    }
    template <typename tuple_type>
    void order_out(tuple_type& res, const tuple_type& in) const {
        res[First] = in[Pos];
        aux_order<Pos + 1, Rest...>().order_out(res, in);
    }
};

template <unsigned Pos>
struct aux_order<Pos> {
    template <typename tuple_type>
    void order_in(tuple_type&, const tuple_type&) const {}
    template <typename tuple_type>
    void order_out(tuple_type&, const tuple_type&) const {}
};

template <typename Index>
struct order;

template <unsigned... Columns>
struct order<index<Columns...>> {
    template <typename tuple_type>
    void order_in(tuple_type& res, const tuple_type& in) const {
        aux_order<0, Columns...>().order_in(res, in);
    }
    template <typename tuple_type>
    void order_out(tuple_type& res, const tuple_type& in) const {
        aux_order<0, Columns...>().order_out(res, in);
    }
};

/**
 * A trie index is like a direct index storing the indexed elements directly
 * within the maintained index structure. However, unlike the B-Tree utilized
 * by a direct index, a Trie is utilized.
 *
 * @tparam Index .. the index to be internally utilized.
 */
template <typename Index>
class TrieIndex {
    typedef Trie<Index::size> tree_type;

    typedef typename tree_type::entry_type tuple_type;

    tree_type data;

public:
    typedef typename tree_type::op_context operation_hints;

    bool empty() const {
        return data.empty();
    }

    std::size_t size() const {
        return data.size();
    }

    bool contains(const tuple_type& tuple, operation_hints& ctxt) const {
        return data.contains(orderIn(tuple), ctxt);
    }

    bool insert(const tuple_type& tuple, operation_hints& ctxt) {
        // the Trie-insert is synchronized internally
        return data.insert(orderIn(tuple), ctxt);
    }

    void insertAll(const TrieIndex& other) {
        // use trie merge
        data.insertAll(other.data);
    }

    void clear() {
        data.clear();
    }

    // ---------------------------------------------
    //                Iterators
    // ---------------------------------------------

    class iterator : public std::iterator<std::forward_iterator_tag, tuple_type> {
        typedef typename tree_type::iterator nested_iterator;

        // the wrapped iterator
        nested_iterator nested;

        // the value currently pointed to
        tuple_type value;

    public:
        // default constructor -- creating an end-iterator
        iterator() {}

        iterator(const nested_iterator& iter) : nested(iter), value(orderOut(*iter)) {}

        // a copy constructor
        iterator(const iterator& other) = default;

        // an assignment operator
        iterator& operator=(const iterator& other) = default;

        // the equality operator as required by the iterator concept
        bool operator==(const iterator& other) const {
            // equivalent if pointing to the same value
            return nested == other.nested;
        }

        // the not-equality operator as required by the iterator concept
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        // the deref operator as required by the iterator concept
        const tuple_type& operator*() const {
            return value;
        }

        // support for the pointer operator
        const tuple_type* operator->() const {
            return &value;
        }

        // the increment operator as required by the iterator concept
        iterator& operator++() {
            ++nested;
            value = orderOut(*nested);
            return *this;
        }
    };

    iterator begin() const {
        return iterator(data.begin());
    }

    iterator end() const {
        return iterator(data.end());
    }

    std::vector<range<iterator>> partition() const {
        // wrap partitions up in re-order iterators
        std::vector<range<iterator>> res;
        for (const auto& cur : data.partition(10000)) {
            res.push_back(make_range(iterator(cur.begin()), iterator(cur.end())));
        }
        return res;
    }

    iterator find(const tuple_type& key, operation_hints& ctxt) const {
        return iterator(data.find(orderIn(key), ctxt));
    }

    template <typename SubIndex>
    range<iterator> equalRange(const tuple_type& tuple, operation_hints& ctxt) const {
        static_assert(is_compatible_with<SubIndex, Index>::value, "Invalid sub-index query!");
        auto r = data.template getBoundaries<SubIndex::size>(orderIn(tuple), ctxt);
        return make_range(iterator(r.begin()), iterator(r.end()));
    }

    static void printDescription(std::ostream& out) {
        out << "trie-index(" << Index() << ")";
    }

private:
    static tuple_type orderIn(const tuple_type& tuple) {
        tuple_type res;
        order<Index>().order_in(res, tuple);
        return res;
    }

    static tuple_type orderOut(const tuple_type& tuple) {
        tuple_type res;
        order<Index>().order_out(res, tuple);
        return res;
    }
};
/**
 * A disjoint index is like a direct index storing the indexed elements directly
 * within the maintained index structure. However, unlike the B-Tree utilized
 * by a direct index, a disjoint set is utilized, with pairs being implicit.
 *
 * @tparam Index .. the index to be internally utilized.
 */
template <typename Index>
class DisjointSetIndex {
    typedef typename ram::Tuple<RamDomain, 2> tuple_type;

    typedef BinaryRelation<tuple_type> data_type;

    data_type data;

public:
    typedef typename data_type::operation_hints operation_hints;

    bool empty() const {
        return data.size() == 0;
    }

    /* returns the number of pairs (NOT the number of elements in the domain, but the number of possibly
     * enumerated pairs!) */
    std::size_t size() const {
        return data.size();
    }

    /**
     * Checks whether the data structure contains this tuple
     * @pararm tuple The pair to check for
     * @param ctxt context hints to help the query process
     * @return true if the data structure contains this tuple
     */
    bool contains(const tuple_type& tuple, operation_hints& ctxt) const {
        // TODO pnappa: optimisations would include ctxt for .contains()
        // doesn't appear to make much sense for BinaryRelation, but future optmisations may be made here
        return data.contains(tuple[0], tuple[1]);
    }

    /**
     * Inserts the pair into the equivalence relation
     * @param tuple the tuple that contains the value to insert into the data structure
     * @param ctxt a context hint which when provided, provides hints to the insertion process
     *      with the intention of speeding up insertion
     * @return true if the tuple has not existed in the data structure already
     */
    bool insert(const tuple_type& tuple, operation_hints& ctxt) {
        return data.insert(tuple[0], tuple[1], ctxt);
    }

    /**
     * Inserts all pairs from the other disjoint set into this one
     * @param other the supplied disjoint set to copy pairs from
     */
    void insertAll(const DisjointSetIndex& other) {
        data.insertAll(other.data);
    }

    /* deletes all data contained in the disjoint-set data structure */
    void clear() {
        data.clear();
    }

    // ---------------------------------------------
    //                Iterators
    // ---------------------------------------------

    class iterator : public std::iterator<std::forward_iterator_tag, tuple_type> {
        typedef typename data_type::iterator nested_iterator;

        // the wrapped iterator
        nested_iterator nested;

        // the value currently pointed to
        tuple_type value;

    public:
        // default constructor -- creating an end-iterator
        iterator(){};

        iterator(const nested_iterator& iter) : nested(iter), value(orderOut(*iter)){};

        // a copy constructor
        iterator(const iterator& other) = default;

        // an assignment operator
        iterator& operator=(const iterator& other) = default;

        // the equality operator as required by the iterator concept
        bool operator==(const iterator& other) const {
            // equivalent if pointing to the same value
            return nested == other.nested;
        }

        // the not-equality operator as required by the iterator concept
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        // the deref operator as required by the iterator concept
        const tuple_type& operator*() const {
            return value;
        }

        // support for the pointer operator
        const tuple_type* operator->() const {
            return &value;
        }

        // the increment operator as required by the iterator concept
        iterator& operator++() {
            ++nested;
            value = orderOut(*nested);
            return *this;
        }
    };

    /** non-const due to binrel find fns */
    iterator begin() const {
        return iterator(data.begin());
    }

    /** non-const due to binrel find fns */
    iterator end() const {
        return iterator(data.end());
    }

    /**
     * Return a list of iterators s.t. we can process in parallel
     * The ordering is not important.
     * The figure 400 is an approximate useful figure, and should be tuned later by another contributor
     * @param np the number of partitions to attempt to return
     * @return a list of iterators over each partition to return
     */
    std::vector<range<iterator>> partition(std::size_t np = 400) const {
        std::vector<range<iterator>> ret;
        auto val = data.partition(np);

        for (auto& x : val) {
            ret.push_back(make_range(iterator(x.begin()), iterator(x.end())));
        }

        return ret;
    }

    /**
     * Find the requested tuple in the data structure, and generate an iterator that begins at that point
     * @param key the tuple to search for
     * @param ctxt the context hint to provide help in finding this position
     * @return an iterator that begins at that position.. and I'm pretty sure if the tuple does not exist,
     * then an iterator == this->end()
     */
    iterator find(const tuple_type& key, operation_hints& ctxt) const {
        // TODO: utilise the ctxt for this
        return iterator(data.find(orderIn(key)));
    }

    template <typename SubIndex>
    range<iterator> equalRange(const tuple_type& tuple, operation_hints& ctxt) const {
        // TODO: unimplemented
        std::cout << "poor call\n";
        std::cerr << "poor call\n";
        throw "unimplemented";
        return make_range(iterator(begin()), iterator(end()));
    }

    static void printDescription(std::ostream& out) {
        out << "disjoint-set-index(" << Index() << ")";
    }

private:
    static tuple_type orderIn(const tuple_type& tuple) {
        tuple_type res;
        order<Index>().order_in(res, tuple);
        return res;
    }

    static tuple_type orderOut(const tuple_type& tuple) {
        tuple_type res;
        order<Index>().order_out(res, tuple);
        return res;
    }
};
// -------------------------------------------------------------

/* A direct index factory only supporting direct indices */
template <typename T, typename Index, bool complete>
struct direct_index_factory;

/* This factory is only defined for full indices */
template <typename T, typename Index>
struct direct_index_factory<T, Index, true> {
    // the arity of the tuple type determines the type of index
    typedef typename std::conditional<T::arity <= 2,  // if the arity is <= 2
            TrieIndex<Index>,                         // .. we use the faster Trie index
            DirectIndex<T, Index>                     // .. otherwise we fall back to the B-Tree index
            >::type type;
    //        typedef DirectIndex<T,Index> type;
};

// -------------------------------------------------------------

/**
 * A factory determining the kind of index structure
 * to be utilized for building up indices over relations
 *
 * @tparam T .. the tuple type to be indexed
 * @tparam Index .. the index order to be realized
 * @tparam complete .. flag indicating whether the resulting index is utilized
 * 					as a complete or partial index (covering all fields or not)
 */
template <typename T, typename Index, bool complete>
struct index_factory;

/* The index structure selection for complete indices. */
template <typename T, typename Index>
struct index_factory<T, Index, true> {
    // pick direct or indirect indexing based on size of tuple
    typedef typename std::conditional<sizeof(T) <= 2 * sizeof(void*),  // if tuple is not bigger than a bound
            typename direct_index_factory<T, Index,
                                              true>::type,  // use a direct index
            IndirectIndex<T,
                                              Index>  // otherwise use an indirect, pointer based index
            >::type type;
};

/* The index structure selection for partial indices. */
template <typename T, typename Index>
struct index_factory<T, Index, false> {
    // if the index is not complete => use indirect multi-set index
    typedef IndirectIndex<T, Index> type;
};

// -------------------------------------------------------------

/**
 * The container representing the actual index structure over a given
 * relation. Each instance is a recursively nested structure where each
 * level covers a different index.
 *
 * @tparam T .. the tuple type to index
 * @tparam IndexFactory .. a factory determining the type of index structure
 * 					to be utilized for each layer
 * @tparam List .. the indices to be covered
 */
template <typename T, template <typename V, typename I, bool> class IndexFactory, typename... List>
class Indices;

/* The recursive implementation of an Indices structure. */
template <typename T, template <typename V, typename I, bool> class IndexFactory, typename First,
        typename... Rest>
class Indices<T, IndexFactory, First, Rest...> {
    // exposes the arity of the stored tuples
    enum { arity = T::arity };

    // determines the type of the index of this level
    typedef typename IndexFactory<T, First, (int)First::size == (int)arity>::type index_t;

    // fixes the type of the nested structure
    typedef Indices<T, IndexFactory, Rest...> nested_indices;

    // -------------------------------------------------------------
    // nested containers
    nested_indices nested;

    // this index
    index_t index;

public:
    // the iterator type for indices on this level
    typedef typename index_t::iterator iterator;

    // a type trait to determine the iterator type for this or a nested index
    template <typename Index>
    struct iter_type {
        typedef typename std::conditional<is_compatible_with<Index, First>::value, iterator,
                typename nested_indices::template iter_type<Index>::type>::type type;
    };

    // an operation context for operations on this index
    struct operation_context {
        // the operation context of this level
        typename index_t::operation_hints ctxt;
        // the operation context of nested levels
        typename nested_indices::operation_context nested;
        // obtains the operation hint for this index
        typename index_t::operation_hints& getForIndex(const First&) {
            return ctxt;
        }
        // obtains the operation hint for nested indices
        template <typename Index>
        auto getForIndex(const Index& i) -> decltype(nested.getForIndex(i)) {
            return nested.getForIndex(i);
        }
    };

    // a utility to verify whether a given index is covered or not
    template <typename I>
    struct is_covered {
        enum { value = is_compatible_with<I, First>::value || nested_indices::template is_covered<I>::value };
    };

    void insert(const T& tuple, operation_context& c) {
        index.insert(tuple, c.ctxt);
        nested.insert(tuple, c.nested);
    }

    void insertAll(const Indices& other) {
        index.insertAll(other.index);
        nested.insertAll(other.nested);
    }

    bool contains(const T& tuple, const First&, operation_context& c) const {
        return index.contains(tuple, c.ctxt);
    }

    template <typename Index>
    bool contains(const T& tuple, const Index& i, operation_context& c) const {
        return nested.contains(tuple, i, c.nested);
    }

    range<iterator> scan(const First&) const {
        return make_range(index.begin(), index.end());
    }

    template <typename Index>
    auto scan(const Index& i) const -> decltype(nested.scan(i)) {
        return nested.scan(i);
    }

    template <typename Index>
    typename std::enable_if<is_compatible_with<Index, First>::value,
            range<typename iter_type<Index>::type>>::type
    equalRange(const T& tuple, operation_context& c) const {
        return index.template equalRange<Index>(tuple, c.ctxt);
    }

    template <typename Index>
    typename std::enable_if<!is_compatible_with<Index, First>::value,
            range<typename iter_type<Index>::type>>::type
    equalRange(const T& tuple, operation_context& c) const {
        return nested.template equalRange<Index>(tuple, c.nested);
    }

    void clear() {
        index.clear();
        nested.clear();
    }

    index_t& getIndex(const First&) {
        return index;
    }

    const index_t& getIndex(const First&) const {
        return index;
    }

    template <typename Index>
    auto getIndex(const Index& i) -> decltype(nested.getIndex(i)) {
        return nested.getIndex(i);
    }

    template <typename Index>
    auto getIndex(const Index& i) const -> decltype(nested.getIndex(i)) {
        return nested.getIndex(i);
    }

    std::vector<range<iterator>> partition(const First&) const {
        // create partition on this level
        return index.partition();
    }

    template <typename I>
    auto partition(const I& index) const -> decltype(nested.partition(index)) {
        return nested.partition(index);
    }

    // prints a description of the organization of this index
    std::ostream& printDescription(std::ostream& out = std::cout) const {
        index_t::printDescription(out);
        out << " ";
        nested.printDescription(out);
        return out;
    }
};

/* The based-case of indices containing no more nested indices. */
template <typename T, template <typename V, typename I, bool> class IndexFactory>
class Indices<T, IndexFactory> {
public:
    typedef int iterator;

    // a type trait to determine the iterator type for this or a nested index
    template <typename Index>
    struct iter_type {
        typedef iterator type;
    };

    struct operation_context {
        template <typename Index>
        int getForIndex(const Index& i) {
            assert(false && "Requested Index not available!");
            return 0;
        }
    };

    template <typename I>
    struct is_covered {
        enum { value = false };
    };

    void insert(const T&, operation_context&) {}

    void insertAll(const Indices&) {}

    template <typename Index>
    bool contains(const T&, const Index&, operation_context&) const {
        assert(false && "Requested Index not available!");
        return false;
    }

    template <typename Index>
    range<iterator> scan(const Index&) const {
        assert(false && "Requested Index not available!");
        return make_range(0, 0);
    }

    template <typename Index>
    range<iterator> equalRange(const T&, const Index&, operation_context&) const {
        assert(false && "Requested Index not available!");
        return make_range(0, 0);
    }

    void clear() {}

    template <typename I>
    std::vector<range<iterator>> partition(const I&) const {
        assert(false && "No Index to partition!");
        return std::vector<range<iterator>>();
    }

    template <typename Index>
    int getIndex(const Index& i) {
        assert(false && "Requested Index not available!");
        return 0;
    }

    std::ostream& printDescription(std::ostream& out = std::cout) const {
        return out;
    }
};
}

namespace iterator_utils {

/**
 * A generic iterator realizing a range query utilizing a scan, by filtering out invalid
 * elements. This iterator is utilized in cases where no index supporting a query is available.
 */
template <typename Iter, typename Index>
class filter_iterator : public std::iterator<std::forward_iterator_tag, typename Iter::value_type> {
    // the type of the tuple visited by this iterator
    using tuple_type = typename Iter::value_type;

    // the begin of the range to be scanned
    Iter iter;

    // the end of the range to be scanned
    Iter end;

    // the comparator realizing the semantic of the Index pattern
    typename Index::comparator comp;

    // the pattern to be scanned for (in combination with the Index template parameter)
    tuple_type value;

public:
    filter_iterator() {}

    filter_iterator(Iter&& begin, Iter&& end, const tuple_type& value)
            : iter(std::move(begin)), end(std::move(end)), value(value) {
        // move to first matching element
        forward();
    }

    // default copy constructor
    filter_iterator(const filter_iterator&) = default;

    // a default move-constructor
    filter_iterator(filter_iterator&&) = default;

    // the default move-assignment operator
    filter_iterator& operator=(filter_iterator&&) = default;

    // the default copy-assignment operator
    filter_iterator& operator=(const filter_iterator&) = default;

    // the equality operator as required by the iterator concept
    bool operator==(const filter_iterator& other) const {
        return iter == other.iter && comp.equal(value, other.value);
    }

    // the not-equality operator as required by the iterator concept
    bool operator!=(const filter_iterator& other) const {
        return !(*this == other);
    }

    // the deref operator as required by the iterator concept
    const tuple_type& operator*() const {
        return *iter;
    }

    // support for the pointer operator
    const tuple_type* operator->() const {
        return &*iter;
    }

    // the increment operator as required by the iterator concept
    filter_iterator& operator++() {
        // move at least one
        ++iter;
        // move to next fitting element
        forward();
        // done
        return *this;
    }

private:
    // move iterator forward to next fitting place
    void forward() {
        // tests whether the current location is a fitting position
        while (iter != end && !comp.equal(*iter, value)) {
            // otherwise move forward
            ++iter;
        }
    }
};

}  // end namespace iterator utils

}  // end namespace ram

}  // end namespace souffle
