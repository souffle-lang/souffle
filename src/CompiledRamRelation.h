/*
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights reserved
 * 
 * The Universal Permissive License (UPL), Version 1.0
 * 
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the 
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified 
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 * 
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 * 
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and 
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the 
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 * 
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/************************************************************************
 *
 * @file CompiledRamRelation.h
 *
 * The central file covering the data structure utilized by
 * the souffle compiler for representing relations in compiled queries.
 *
 ***********************************************************************/

#pragma once

#include <iterator>
#include <type_traits>
#include <mutex>
#include <array>
#include <libgen.h>

#include "Util.h"
#include "IterUtils.h"
#include "Table.h"
#include "BTree.h"
#include "Trie.h"
#include "CompiledRamTuple.h"
#include "SymbolTable.h"
#include "ParallelUtils.h"

namespace ram {

// -------------------------------------------------------------
//                             Relation
// -------------------------------------------------------------

/**
 * The declaration of the generic template class defined within
 * this header file. The actual implementation may be specialized
 * for various arities / index combinations.
 *
 * @tparam arity    ... the arity of the represented relation
 * @tparam Indicies ... a list of indices to be maintained for fast access
 */
template<unsigned arity, typename ... Indices>
class Relation;



/**
 * A namespace enclosing template-meta-programming utilities for handling
 * parameter lists for templates.
 */
namespace column_utils {

    // -- checks whether a given number is contained in a list of numbers --

    template<unsigned E, unsigned ... List>
    struct contains;

    template<unsigned E>
    struct contains<E> {
        enum { value = false };
    };

    template<unsigned E, unsigned F, unsigned ... Rest>
    struct contains<E,F,Rest...> {
        enum { value = (E==F) || contains<E,Rest...>::value };
    };


    // -- check uniqueness of a list of integer values --

    template<unsigned ... List>
    struct unique;

    template<>
    struct unique<> {
        enum { value = true };
    };

    template<unsigned E, unsigned ... Rest>
    struct unique<E,Rest...> {
        enum { value = !contains<E,Rest...>::value && unique<Rest...>::value };
    };

} // end namespace column_utils

/**
 * A namespace enclosing utilities required by indices.
 */
namespace index_utils {

	// -------- generic tuple comparator ----------

    template<unsigned ... Columns>
    struct comparator;

    template<unsigned First, unsigned ... Rest>
    struct comparator<First,Rest...> {
        template<typename T>
        int operator()(const T& a, const T& b) const {
            return (a[First] < b[First]) ? -1 : ((a[First] > b[First]) ? 1 : comparator<Rest...>()(a,b));
        }
        template<typename T>
        bool less(const T& a, const T& b) const {
            return a[First] < b[First] || (a[First] == b[First] && comparator<Rest...>().less(a,b));
        }
        template<typename T>
        bool equal(const T& a, const T& b) const {
            return a[First] == b[First] && comparator<Rest...>().equal(a,b);
        }
    };

    template<>
    struct comparator<> {
        template<typename T>
        int operator()(const T& a, const T& b) const {
            return 0;
        }
        template<typename T>
        bool less(const T& a, const T& b) const {
            return false;
        }
        template<typename T>
        bool equal(const T& a, const T& b) const {
            return true;
        }
    };


	// ----- a comparator wrapper dereferencing pointers ----------
    //         (required for handling indirect indices)

    template<typename Comp>
    struct deref_compare {
        template<typename T>
        int operator()(const T& a, const T& b) const {
            Comp comp;
            return comp(*a,*b);
        }
        template<typename T>
        bool less(const T& a, const T& b) const {
            Comp comp;
            return comp.less(*a,*b);
        }
        template<typename T>
        bool equal(const T& a, const T& b) const {
            Comp comp;
            return comp.equal(*a,*b);
        }
    };


    // ----- a utility for printing lists of parameters -------
    //    (required for printing descriptions of relations)

    template<unsigned ... Columns>
    struct print;

    template<>
    struct print<> {
        friend std::ostream& operator<<(std::ostream& out, const print&) {
            return out;
        }
    };

    template<unsigned Last>
    struct print<Last> {
        friend std::ostream& operator<<(std::ostream& out, const print&) {
            return out << Last;
        }
    };

    template<unsigned First, unsigned Second, unsigned ... Rest>
    struct print<First, Second, Rest...> {
        friend std::ostream& operator<<(std::ostream& out, const print&) {
            return out << First << "," << print<Second, Rest...>();
        }
    };

} // end namespace utils


/**
 * The index class is utilized as a template-meta-programming structure
 * to specify and realize indices.
 *
 * @tparam Columns ... the order in which elements of the relation to be indexed
 * 				shell be considered by this index.
 */
template<unsigned ... Columns>
struct index {

    // check validity of this index - column fields need to be unique
    static_assert(column_utils::unique<Columns...>::value, "Invalid duplication of columns!");

    // the comparator associated to this index
    typedef index_utils::comparator<Columns...> comparator;

    // enables to check whether the given column is covered by this index or not
    template<unsigned Col>
    struct covers {
        enum { value = column_utils::contains<Col, Columns...>::value };
    };

    // the length of the index
    enum {
        size = sizeof...(Columns)
    };

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

    template<typename ... Index>
    struct all_indices {
        enum { value = false };
    };

    template<>
    struct all_indices<> {
        enum { value = true };
    };

    template<unsigned ... Columns, typename ... Rest>
    struct all_indices<index<Columns...>,Rest...> {
        enum { value = all_indices<Rest...>::value };
    };


    // -- checks whether a list of typed contains a certain type --

    template<typename E, typename ... List>
    struct contains;

    template<typename E>
    struct contains<E> {
        enum { value = false };
    };

    template<typename E, typename F, typename ... Rest>
    struct contains<E,F,Rest...> {
        enum { value = contains<E,Rest...>::value };
    };

    template<typename E, typename ... Rest>
    struct contains<E,E,Rest...> {
        enum { value = true };
    };


    // -- check whether a given list is a list of unique indices --

    template<typename ... Index>
    struct unique;

    template<>
    struct unique<> {
        enum { value = true };
    };

    template<typename First, typename ... Rest>
    struct unique<First,Rest...> {
        enum { value = all_indices<First,Rest...>::value && !contains<First,Rest...>::value };
    };


    // -- check whether the columns of an index are not exceeding a given arity --

    template<unsigned arity, typename Index>
    struct check_index_arity {
        enum { value = false };
    };

    template<unsigned arity>
    struct check_index_arity<arity,index<>> {
        enum { value = true };
    };

    template<unsigned arity, unsigned F, unsigned ... Rest>
    struct check_index_arity<arity,index<F,Rest...>> {
        enum { value = (F < arity) && check_index_arity<arity,index<Rest...>>::value };
    };


    // -- check whether the columns of a list of indices are not exceeding a given arity --

    template<unsigned arity, typename ... Index>
    struct check_arity;

    template<unsigned arity>
    struct check_arity<arity> {
        enum { value = true };
    };

    template<unsigned arity, typename F, typename ... Rest>
    struct check_arity<arity,F,Rest...> {
        enum { value = check_index_arity<arity,F>::value && check_arity<arity,Rest...>::value };
    };


    // -- checks the validity of a given list of indices --

    template<unsigned arity, typename ... Indices>
    struct check {
        enum { value =
            unique<Indices...>::value && 				// indices need to be unique
			check_arity<arity,Indices...>::value		// and valid
        };
    };


    // -- a utility extending a given index by another column --
    //   e.g. index<1,0>   =>    index<1,0,2>

    template<typename Index, unsigned column>
    struct extend;

    template<unsigned ... Columns, unsigned Col>
    struct extend<index<Columns...>,Col> {
        typedef index<Columns...,Col> type;
    };


    // -- a utility concatenating indices --

    template<typename I1, typename I2>
    struct concat;

    template<unsigned ... C1, unsigned ... C2>
    struct concat<index<C1...>,index<C2...>> {
        typedef index<C1...,C2...> type;
    };


    // -- obtains a full index for a given arity --

    template<unsigned arity>
    struct get_full_index {
        typedef typename extend<
                typename get_full_index<arity-1>::type,
                arity-1
        >::type type;
    };

    template<>
    struct get_full_index<0> {
        typedef index<> type;
    };


    // -- extends a given index to a full index --

    namespace detail {

    	// an auxiliary type required to implement index extension
        template<unsigned i, unsigned arity, typename Index>
        struct extend_to_full_index_aux {
            typedef typename extend_to_full_index_aux<i+1,arity,
                    typename std::conditional<
                        (Index::template covers<i>::value),
                        Index,
                        typename extend<Index,i>::type
                    >::type
                >::type type;
        };

        template<unsigned arity, typename Index>
        struct extend_to_full_index_aux<arity,arity,Index> {
            typedef Index type;
        };

    }


    template<unsigned arity, typename Index>
    struct extend_to_full_index : public detail::extend_to_full_index_aux<0,arity,Index> {};


    // -- checks whether one index is a prefix of another index --

    template<typename I1, typename I2>
    struct is_prefix {
        enum { value = false };
    };

    template<unsigned ... Rest>
    struct is_prefix<index<>,index<Rest...>> {
        enum { value = true };
    };

    template<unsigned F, unsigned ... Ra, unsigned ... Rb>
    struct is_prefix<index<F,Ra...>,index<F,Rb...>> {
        enum { value = is_prefix<index<Ra...>,index<Rb...>>::value };
    };


    // -- obtains the prefix of an index --

    namespace detail {

        template<unsigned L, unsigned ... Rest>
        struct get_prefix_aux;

        template<unsigned L, unsigned First, unsigned ... Rest>
        struct get_prefix_aux<L,First,Rest...> {
            typedef typename concat<index<First>,typename get_prefix_aux<L-1,Rest...>::type>::type type;
        };

        template<unsigned First, unsigned ... Rest>
        struct get_prefix_aux<0,First,Rest...> {
            typedef index<> type;
        };

        template<>
        struct get_prefix_aux<0> {
            typedef index<> type;
        };
    }


    template<unsigned L, typename I>
    struct get_prefix;

    template<unsigned L, unsigned ... Rest>
    struct get_prefix<L,index<Rest...>> {
        typedef typename detail::get_prefix_aux<L,Rest...>::type type;
    };


    // -- determines whether the columns of one index is a subset of the columns of another index --

    template<typename I1, typename I2>
    struct is_subset_of {
        enum { value = false };
    };

    template<unsigned First, unsigned ... Rest, unsigned ... Full>
    struct is_subset_of<index<First,Rest...>,index<Full...>> {
        enum { value = column_utils::contains<First,Full...>::value && is_subset_of<index<Rest...>,index<Full...>>::value };
    };

    template<unsigned ... Full>
    struct is_subset_of<index<>,index<Full...>> {
        enum { value = true };
    };


    // -- checks whether one index is a permutation of another index --

    template<typename I1, typename I2>
    struct is_permutation {
        enum { value = false };
    };

    template<unsigned ... C1, unsigned ... C2>
    struct is_permutation<index<C1...>,index<C2...>> {
        enum { value = sizeof...(C1) == sizeof...(C2) && is_subset_of<index<C1...>,index<C2...>>::value };
    };


    // -- checks whether one index is an extension of another index --

    namespace detail {

        template<typename P1, typename R1, typename P2, typename R2>
        struct is_compatible_with_aux {
            enum { value = false };
        };

        template<
            unsigned ... A1, unsigned A, unsigned ... A2,
            unsigned ... B1, unsigned B, unsigned ... B2
        >
        struct is_compatible_with_aux<
            index<A1...>, index<A,A2...>,
            index<B1...>, index<B,B2...>
        > {
            enum { value = is_compatible_with_aux<
                    index<A1...,A>,index<A2...>,
                    index<B1...,B>,index<B2...>
                >::value
            };
        };

        template<unsigned ... A, unsigned ... B, unsigned ... R>
        struct is_compatible_with_aux<index<A...>,index<>,index<B...>,index<R...>> {
            enum { value = is_permutation<index<A...>,index<B...>>::value };
        };
    }

    template<typename I1, typename I2>
    struct is_compatible_with {
        enum { value = false };
    };

    template<unsigned ... C1, unsigned ... C2>
    struct is_compatible_with<index<C1...>,index<C2...>> {
        enum { value = detail::is_compatible_with_aux<index<>,index<C1...>,index<>,index<C2...>>::value };
    };



    // -- check whether there is a full index in a list of indices --

    template<unsigned arity, typename ... Indices>
    struct contains_full_index;

    template<unsigned arity, typename First, typename ... Rest>
    struct contains_full_index<arity, First, Rest...> {
        enum { value = First::size == arity || contains_full_index<arity,Rest...>::value };
    };

    template<unsigned arity>
    struct contains_full_index<arity> {
        enum { value = false };
    };


    // -- get first full index from a list of indices --

    template<unsigned arity, typename ... Indices>
    struct get_first_full_index;

    template<unsigned arity, typename First, typename ... Rest>
    struct get_first_full_index<arity,First,Rest...> {
        typedef typename std::conditional<
            First::size == arity,
            First,
            typename get_first_full_index<arity,Rest...>::type
        >::type type;
    };

    template<unsigned arity, typename Index>
    struct get_first_full_index<arity,Index> {
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
    template<typename From, typename To, RamDomain value>
    struct mask;

    // masks out uncovered columns
    template<unsigned F, unsigned ... R, unsigned ... M, RamDomain value>
    struct mask<index<F,R...>,index<M...>,value> {
        template<typename T>
        void operator()(T& t) const {
            if (!column_utils::contains<F,M...>::value) t[F] = value;
            mask<index<R...>,index<M...>, value>()(t);
        }
    };

    // the terminal case
    template<unsigned ... M, RamDomain value>
    struct mask<index<>,index<M...>, value> {
        template<typename T>
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
    template<typename From, typename To, typename T>
    T lower(T tuple) {
        mask<From,To,MIN_RAM_DOMAIN>()(tuple);
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
    template<typename From, typename To, typename T>
    T raise(T tuple) {
        mask<From,To,MAX_RAM_DOMAIN>()(tuple);
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
    template<typename Tuple, typename Index>
    struct DirectIndex {

        typedef btree_set<Tuple,typename Index::comparator> data_structure;

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

        template<typename SubIndex>
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
            return make_range(
                    index.lower_bound(lower<Index,SubIndex>(key), hints),
                    index.upper_bound(raise<Index,SubIndex>(key), hints)
            );
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
    template<typename Tuple, typename Index>
    struct IndirectIndex {

        typedef typename std::conditional<
                (int)(Tuple::arity) == (int)(Index::size),
                btree_set<const Tuple*,index_utils::deref_compare<typename Index::comparator>>,
                btree_multiset<const Tuple*,index_utils::deref_compare<typename Index::comparator>>
        >::type data_structure;

        typedef typename std::remove_cv<typename std::remove_pointer<typename data_structure::key_type>::type>::type key_type;

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

        template<typename SubIndex>
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
            auto low = lower<Index,SubIndex>(key);
            auto hig = raise<Index,SubIndex>(key);
            return make_range(
                    derefIter(index.lower_bound(&low, hints)),
                    derefIter(index.upper_bound(&hig, hints))
            );
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
            std::vector<range<iterator>> res;
            for(const auto& cur : index.getChunks(400)) {
                res.push_back(make_range(derefIter(cur.begin()), derefIter(cur.end())));
            }
            return res;
        }

        static void printDescription(std::ostream& out) {
            out << "indirect-btree-index(" << Index() << ")";
        }

    };


    // -------------------------------------------------------------

    template<unsigned Pos, unsigned ... Order>
    struct aux_order;

    template<unsigned Pos, unsigned First, unsigned ... Rest>
    struct aux_order<Pos,First,Rest...> {
        template<typename tuple_type>
        void order_in(tuple_type& res, const tuple_type& in) const {
            res[Pos] = in[First];
            aux_order<Pos+1,Rest...>().order_in(res,in);
        }
        template<typename tuple_type>
        void order_out(tuple_type& res, const tuple_type& in) const {
            res[First] = in[Pos];
            aux_order<Pos+1,Rest...>().order_out(res,in);
        }
    };

    template<unsigned Pos>
    struct aux_order<Pos> {
        template<typename tuple_type>
        void order_in(tuple_type& , const tuple_type& ) const {}
        template<typename tuple_type>
        void order_out(tuple_type& , const tuple_type& ) const {}
    };

    template<typename Index> struct order;

    template<unsigned ... Columns>
    struct order<index<Columns...>> {
        template<typename tuple_type>
        void order_in(tuple_type& res, const tuple_type& in) const {
            aux_order<0,Columns...>().order_in(res,in);
        }
        template<typename tuple_type>
        void order_out(tuple_type& res, const tuple_type& in) const {
            aux_order<0,Columns...>().order_out(res,in);
        }
    };

    /**
     * A trie index is like a direct index storing the indexed elements directly
     * within the maintained index structure. However, unlike the B-Tree utilized
     * by a direct index, a Trie is utilized.
     *
     * @tparam Index .. the index to be internally utilized.
     */
    template<typename Index>
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

        class iterator : public std::iterator<std::forward_iterator_tag,tuple_type> {

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
            iterator& operator=(const iterator& other) =default;

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
            for(const auto& cur : data.partition(10000)) {
                res.push_back(make_range(iterator(cur.begin()), iterator(cur.end())));
            }
            return res;
        }

        iterator find(const tuple_type& key, operation_hints& ctxt) const {
            return iterator(data.find(orderIn(key), ctxt));
        }

        template<typename SubIndex>
        range<iterator> equalRange(const tuple_type& tuple, operation_hints& ctxt) const {
            static_assert(is_compatible_with<SubIndex,Index>::value, "Invalid sub-index query!");
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

    // -------------------------------------------------------------


    /* A direct index factory only supporting direct indices */
    template<typename T, typename Index, bool complete>
    struct direct_index_factory;

    /* This factory is only defined for full indices */
    template<typename T, typename Index>
    struct direct_index_factory<T,Index,true> {
        // the arity of the tuple type determines the type of index
        typedef typename std::conditional<
            T::arity <= 2,            // if the arity is <= 2
            TrieIndex<Index>,         // .. we use the faster Trie index
            DirectIndex<T,Index>      // .. otherwise we fall back to the B-Tree index
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
    template<typename T, typename Index, bool complete>
    struct index_factory;

    /* The index structure selection for complete indices. */
    template<typename T, typename Index>
    struct index_factory<T,Index,true> {
        // pick direct or indirect indexing based on size of tuple
        typedef typename std::conditional<
            sizeof(T) <= 2 * sizeof(void*),                         // if the tuple is not bigger than a certain boundary
            typename direct_index_factory<T,Index,true>::type,      // use a direct index
            IndirectIndex<T,Index>                                  // otherwise we use an indirect, pointer based index
        >::type type;
    };

    /* The index structure selection for partial indices. */
    template<typename T, typename Index>
    struct index_factory<T,Index,false> {
        // if the index is not complete => use indirect multi-set index
        typedef IndirectIndex<T,Index> type;
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
    template<
        typename T,
        template<typename V, typename I, bool> class IndexFactory,
        typename ... List
    >
    class Indices;

    /* The recursive implementation of an Indices structure. */
    template<
        typename T,
        template<typename V, typename I, bool> class IndexFactory,
        typename First, typename ... Rest
    >
    class Indices<T,IndexFactory,First,Rest...> {

    	// exposes the arity of the stored tuples
        enum { arity = T::arity };

        // determines the type of the index of this level
        typedef typename IndexFactory<T, First, (int)First::size == (int)arity>::type index_t;

        // fixes the type of the nested structure
        typedef Indices<T,IndexFactory,Rest...> nested_indices;

        // nested containers
        nested_indices nested;

        // this index
        index_t index;

    public:

        // the iterator type for indices on this level
        typedef typename index_t::iterator iterator;

        // a type trait to determine the iterator type for this or a nested index
        template<typename Index>
        struct iter_type {
            typedef typename std::conditional<
                    is_compatible_with<Index,First>::value,
                    iterator,
                    typename nested_indices::template iter_type<Index>::type
                >::type type;
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
            template<typename Index>
            auto getForIndex(const Index& i) -> decltype(nested.getForIndex(i)) {
                return nested.getForIndex(i);
            }
        };

        // a utility to verify whether a given index is covered or not
        template<typename I>
        struct is_covered {
            enum { value = is_compatible_with<I,First>::value || nested_indices::template is_covered<I>::value };
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

        template<typename Index>
        bool contains(const T& tuple, const Index& i, operation_context& c) const {
            return nested.contains(tuple,i,c.nested);
        }

        range<iterator> scan(const First&) const {
            return make_range(index.begin(),index.end());
        }

        template<typename Index>
        auto scan(const Index& i) const -> decltype(nested.scan(i)) {
            return nested.scan(i);
        }

        template<typename Index>
        typename std::enable_if<
            is_compatible_with<Index,First>::value,
            range<typename iter_type<Index>::type>
        >::type
        equalRange(const T& tuple, operation_context& c) const {
            return index.template equalRange<Index>(tuple, c.ctxt);
        }

        template<typename Index>
        typename std::enable_if<
            !is_compatible_with<Index,First>::value,
            range<typename iter_type<Index>::type>
        >::type
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

        template<typename Index>
        auto getIndex(const Index& i) -> decltype(nested.getIndex(i)) {
            return nested.getIndex(i);
        }

        template<typename Index>
        auto getIndex(const Index& i) const -> decltype(nested.getIndex(i)) {
            return nested.getIndex(i);
        }

        std::vector<range<iterator>> partition(const First&) const {
            // create partition on this level
            return index.partition();
        }

        template<typename I>
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
    template<
        typename T,
        template<typename V, typename I, bool> class IndexFactory
    >
    class Indices<T,IndexFactory> {
    public:

        typedef int iterator;

        // a type trait to determine the iterator type for this or a nested index
        template<typename Index>
        struct iter_type {
            typedef iterator type;
        };

        struct operation_context {
            template<typename Index>
            int getForIndex(const Index& i) {
                assert(false && "Requested Index not available!");
                return 0;
            }
        };

        template<typename I>
        struct is_covered {
            enum { value = false };
        };

        void insert(const T&, operation_context&) { }

        void insertAll(const Indices&) { }

        template<typename Index>
        bool contains(const T&, const Index&, operation_context&) const {
            assert(false && "Requested Index not available!");
            return false;
        }

        template<typename Index>
        range<iterator> scan(const Index&) const {
            assert(false && "Requested Index not available!");
            return make_range(0,0);
        }

        template<typename Index>
        range<iterator> equalRange(const T&, const Index&, operation_context&) const {
            assert(false && "Requested Index not available!");
            return make_range(0,0);
        }

        void clear() {}

        template<typename I>
        std::vector<range<iterator>> partition(const I&) const {
            assert(false && "No Index to partition!");
            return std::vector<range<iterator>>();
        }

        template<typename Index>
        int getIndex(const Index& i) {
            assert(false && "Requested Index not available!");
            return 0;
        }

        std::ostream& printDescription(std::ostream& out = std::cout) const {
            return out;
        }
    };

}


/**
 * A base class for partially specialized relation templates following below.
 * This base class provides generic interfaces and adapters forwarding requests
 * to the most general version offered by the derived classes to save implementation
 * overhead and to unify the interface.
 *
 * @tparam arity .. the arity of the resulting relation
 * @tparam Derived .. the type of the derived relation
 */
template<unsigned arity, typename Derived>
struct RelationBase {
    using SymbolTable = souffle::SymbolTable; // XXX pending namespace cleanup

	// the type of tuple maintained by this relation
    typedef Tuple<RamDomain,arity> tuple_type;

    // -- contains wrapper --

    template<typename ... Args>
    bool contains(Args ... args) const {
        RamDomain data[arity] = {RamDomain(args)...};
        return static_cast<const Derived*>(this)->contains(reinterpret_cast<const tuple_type&>(data));
    }

    bool contains(const tuple_type& tuple) const {
        typename Derived::operation_context ctxt;
        return static_cast<const Derived*>(this)->contains(tuple, ctxt);
    }

    // -- insert wrapper --

    template<typename ... Args>
    bool insert(Args ... args) {
        RamDomain data[arity] = {RamDomain(args)...};
        return static_cast<Derived*>(this)->insert(reinterpret_cast<const tuple_type&>(data));
    }

    bool insert(const tuple_type& tuple) {
        typename Derived::operation_context ctxt;
        return static_cast<Derived*>(this)->insert(tuple,ctxt);
    }

    // -- IO --

    /* a utility struct for load/print operations */
    struct SymbolMask {
        int mask[arity];
        bool isSymbol(unsigned x) const {
            assert(x < arity);
            return mask[x] != 0;
        }
    };

    /* prints this relation to the given file in CSV format */
    void printCSV(const char* fn, const SymbolTable& symbolTable, const SymbolMask& format) const {
        // support NULL as an output
        if (fn == nullptr) {
            printCSV(std::cout, symbolTable, format);
            return;
        }
        // open output file
        std::ofstream fos(fn);
        printCSV(fos, symbolTable, format);
    }

    /* print table in csv format */
    void printCSV(std::ostream& out, const SymbolTable& symbolTable, const SymbolMask& format) const {
        /* print table */
        for(const tuple_type& cur : asDerived()) {
            for(unsigned i=0; i<arity; i++) {
                if (format.isSymbol(i)) {
                    out << symbolTable.resolve(cur[i]);
                } else {
                    out << (int32_t) cur[i];
                }
                if (i+1 != arity) out << '\t';
            }
            out << '\n';
        }
    }

    /**
     * Prints this relation to the given file.
     *
     * @param fn .. the file name to be targeted
     * @param format .. a mask of 0s or 1s determining which components
     * 				of the tuples should be converted to a symbol using
     * 				the symbol table and which are representing actual numbers
     */
    template<typename ... Format>
    void printCSV(const char* fn, const SymbolTable& symbolTable, Format ... format) const {
        printCSV(fn, symbolTable, SymbolMask({int(format)...}));
    }

    /**
     * Prints this relation to the given file.
     *
     * @param fn .. the file name to be targeted
     * @param format .. a mask of 0s or 1s determining which components
     *              of the tuples should be converted to a symbol using
     *              the symbol table and which are representing actual numbers
     */
    template<typename ... Format>
    void printCSV(const std::string& fn, const SymbolTable& symbolTable, Format ... format) const {
        printCSV(fn.c_str(), symbolTable, SymbolMask({int(format)...}));
    }

    /* Loads the tuples form the given file into this relation. */
    void loadCSV(const char* fn, SymbolTable& symbolTable, const SymbolMask& format) {
        // check for null
        if (fn == nullptr) {
            // for completeness ...
            loadCSV(std::cin, symbolTable, format);
            return;
        }

        // open file
        std::ifstream in;
        in.open(fn);
        if (!in.is_open()) {
            std::cerr << "Cannot open file " << fn << "!\n";
            exit(1);        // panic ?!?
        }

        // and parse it
        if(loadCSV(in, symbolTable, format)) {
            char *bname = strdup(fn);
            std::string simplename = basename(bname);
            std::cerr << "Wrong arity of fact file " << simplename << "!\n";
        }
    }

    /* Loads the tuples form the given file into this relation. */
    bool loadCSV(std::istream& in, SymbolTable& symbolTable, const SymbolMask& format) {
        bool error = false;

        // for all the content
        while(!in.eof()) {

            std::string line;
            tuple_type tuple;

            getline(in,line);
            if (in.eof()) break;

            int start, end = -1;
            for(uint32_t col=0;col<arity;col++) {
                start = end+1;
                end = line.find('\t', start);
                if ((size_t)end == std::string::npos) {
                    end = line.length();
                }
                std::string element;
                if (start >=0 && start <=  end && (size_t)end <= line.length() ) {
                    element = line.substr(start,end-start);
                    if (element == "") {
                        element = "n/a";
                    }
                } else {
                    element = "n/a";
                    error = true;
                }
                if (format.isSymbol(col)) {
                    tuple[col] = symbolTable.lookup(element.c_str());
                } else {
                    tuple[col] = atoi(element.c_str());
                }
            }
            if ((size_t)end != line.length()) {
                error = true;
            }
            insert(tuple);
        }
        return error;
    }

    /**
	 * Loads tuples from the given file into this relation.
	 *
	 * @param fn .. the file name to be targeted
	 * @param format .. a mask of 0s or 1s determining which components
	 * 				of the tuples should be interpreted using
	 * 				the symbol table and which are representing actual numbers
	 */
    template<typename ... Format>
    void loadCSV(const char* fn, SymbolTable& symbolTable, Format ... format) {
        loadCSV(fn, symbolTable, SymbolMask({int(format)...}));
    }

    /**
     * Loads tuples from the given file into this relation.
     *
     * @param fn .. the file name to be targeted
     * @param format .. a mask of 0s or 1s determining which components
     *              of the tuples should be interpreted using
     *              the symbol table and which are representing actual numbers
     */
    template<typename ... Format>
    void loadCSV(const std::string& fn, SymbolTable& symbolTable, Format ... format) {
        loadCSV(fn.c_str(), symbolTable, SymbolMask({int(format)...}));
    }

    /* Provides a description of the internal organization of this relation. */
    std::string getDescription() const {
        std::stringstream out;
        static_cast<const Derived*>(this)->printDescription(out);
        return out.str();
    }

private:

    /* Provides type-save access to the members of the derived class. */
    Derived& asDerived() {
        return static_cast<Derived&>(*this);
    }

    /* Provides type-save access to the members of the derived class. */
    const Derived& asDerived() const {
        return static_cast<const Derived&>(*this);
    }

};


/**
 * The most generic implementation of a relation supporting arbitrary arities > 0 and
 * consistent lists of indices.
 *
 * @tparam arity .. the arity of the resulting relation
 * @tparam Indices .. the indices to be maintained on top
 */
template<unsigned arity, typename ... Indices>
class Relation : public RelationBase<arity,Relation<arity,Indices...>> {

	// check validity of indices
    static_assert(index_utils::check<arity,Indices...>::value, "Warning: invalid indices combination!");

    // shortcut for the base class
    typedef RelationBase<arity,Relation<arity,Indices...>> base;

public:

    /* The type of tuple stored in this relation. */
    typedef typename base::tuple_type tuple_type;

    /* The table storing the master-copies of the relations. */
    typedef Table<tuple_type> table_t;

    /* The iterator type to be utilized for relation scans. */
    typedef typename table_t::iterator iterator;

private:

    // obtain type of index collection
    typedef typename std::conditional<
            // check whether there is at least one index covering all columns
            index_utils::contains_full_index<arity, Indices...>::value,
            // if so: just create those indices
            index_utils::Indices<tuple_type,index_utils::index_factory, Indices...>,
            // otherwise: add an additional full index
            index_utils::Indices<tuple_type,index_utils::index_factory, typename index_utils::get_full_index<arity>::type, Indices...>
        >::type indices_t;

    // define the primary index for existence checks
    typedef typename index_utils::get_first_full_index<arity, Indices..., typename index_utils::get_full_index<arity>::type>::type primary_index;

    // the data stored in this relation (main copy, referenced by indices)
    table_t data;

    // all other indices
    indices_t indices;

    // the lock utilized to synchronize inserts
    Lock insert_lock;

    /* A utility to check whether a certain index is covered by this relation. */
    template<typename Index>
    struct covered {
        enum { value = indices_t::template is_covered<Index>::value };
    };

public:

    /* The context information to be utilized by operations on this relation. */
    typedef typename indices_t::operation_context operation_context;

    // import generic signatures from the base class
    using base::insert;
    using base::contains;


    // --- most general implementation ---

    operation_context createContext() {
        return operation_context();
    }

    bool empty() const {
        return data.empty();
    }

    std::size_t size() const {
        return data.size();
    }

    bool contains(const tuple_type& tuple, operation_context& context) const {
        return indices.contains(tuple, primary_index(), context);
    }

    bool insert(const tuple_type& tuple, operation_context& context) {

        // the pointer to the inserted tuple in the table
        const tuple_type* masterCopy = nullptr;

        // check primary index first
        {
            // acquire exclusive access to the primary index
            auto lease = insert_lock.acquire();

            // if already present => skip
            if (contains(tuple, context)) return false;

            // add value to table
            masterCopy = &data.insert(tuple);

            // add tuple to primary index
            indices.getIndex(primary_index()).insert(*masterCopy, context.getForIndex(primary_index()));

            // release lease (automatically)
        }

        // insert into remaining indices (and primary again but the context will make it quick)
        indices.insert(*masterCopy, context);

        // new element has been added
        return true;
    }

    template<typename ... Idxs>
    void insertAll(const Relation<arity,Idxs...>& other) {
        operation_context context;
        for(const tuple_type& cur : other) {
            insert(cur, context);
        }
    }

    template<typename Index>
    auto scan() const -> decltype(indices.scan(Index())) {
        return indices.scan(Index());
    }


    // -- equal range wrapper --

    template<typename Index>
    range<typename indices_t::template iter_type<Index>::type>
    equalRange(const tuple_type& value) const {
        operation_context ctxt;
        return equalRange<Index>(value, ctxt);
    }

    template<typename Index>
    range<typename indices_t::template iter_type<Index>::type>
    equalRange(const tuple_type& value, operation_context& context) const {
        static_assert(covered<Index>::value, "Addressing uncovered index!");
        return indices.template equalRange<Index>(value,context);
    }

    template<unsigned ... Columns>
    range<typename indices_t::template iter_type<index<Columns...>>::type>
    equalRange(const tuple_type& value) const {
        return equalRange<index<Columns...>>(value);
    }

    template<unsigned ... Columns, typename Context>
    range<typename indices_t::template iter_type<index<Columns...>>::type>
    equalRange(const tuple_type& value, Context& ctxt) const {
        return equalRange<index<Columns...>>(value,ctxt);
    }

    iterator begin() const {
        return data.begin();
    }

    iterator end() const {
        return data.end();
    }

    void purge() {
        data.clear();
        indices.clear();
    }

    auto partition() -> decltype(indices.partition(primary_index())) {
        return indices.partition(primary_index());
    }

    /* Prints a description of the internal structure of this relation. */
    std::ostream& printDescription(std::ostream& out = std::cout) const {
        out << "Relation of arity=" << arity << " with indices [ ";
        indices.printDescription(out);
        out << " ] where " << primary_index() << " is the primary index";
        return out;
    }

};

/**
 * A special relation that only utilizes direct indices.
 *
 * @tparam arity .. the arity of the resulting relation
 * @tparam Indices .. the indices to be maintained on top
 */
template<unsigned arity, typename Primary, typename ... Indices>
class DirectIndexedRelation : public RelationBase<arity,Relation<arity,Primary,Indices...>> {

    // check validity of indices
    static_assert(
            index_utils::check<arity,
                typename index_utils::extend_to_full_index<arity, Primary>::type,
                typename index_utils::extend_to_full_index<arity, Indices>::type...
            >::value,
            "Warning: invalid indices combination!");

    // shortcut for the base class
    typedef RelationBase<arity,Relation<arity,Primary,Indices...>> base;

public:
    /* The type of tuple stored in this relation. */
    typedef typename base::tuple_type tuple_type;


private:

    // obtain type of index collection
    typedef typename index_utils::Indices<
                tuple_type,index_utils::direct_index_factory,
                typename index_utils::extend_to_full_index<arity,Primary>::type,
                typename index_utils::extend_to_full_index<arity,Indices>::type...
            > indices_t;

    // define the primary index for existence checks
    typedef typename index_utils::extend_to_full_index<arity,Primary>::type primary_index;

    // all other indices
    indices_t indices;

public:
    /* iterator type */
    typedef decltype(indices.getIndex(primary_index()).begin()) iterator; 

public:

    /* The context information to be utilized by operations on this relation. */
    typedef typename indices_t::operation_context operation_context;

    // import generic signatures from the base class
    using base::insert;
    using base::contains;


    // --- most general implementation ---

    operation_context createContext() {
        return operation_context();
    }

    bool empty() const {
        return indices.getIndex(primary_index()).empty();
    }

    std::size_t size() const {
        return indices.getIndex(primary_index()).size();
    }

    bool contains(const tuple_type& tuple, operation_context& context) const {
        return indices.contains(tuple, primary_index(), context);
    }

    bool insert(const tuple_type& tuple, operation_context& context) {
        // insert in primary index first ...
        if (indices.getIndex(primary_index()).insert(tuple, context.getForIndex(primary_index()))) {
            // and if new, to all other indices
            indices.insert(tuple, context);
            // this was a new element
            return true;
        }
        // no new element
        return false;
    }

    void insertAll(const DirectIndexedRelation& other) {
        // merge indices using index-specific implementation
        indices.insertAll(other.indices);
    }

    template<typename ... Idxs>
    void insertAll(const Relation<arity,Idxs...>& other) {
        operation_context context;
        for(const tuple_type& cur : other) {
            insert(cur, context);
        }
    }

    template<typename Index>
    auto scan() const -> decltype(indices.scan(Index())) {
        return indices.scan(Index());
    }

    template<typename Index>
    range<typename indices_t::template iter_type<Index>::type>
    equalRange(const tuple_type& value) const {
        operation_context ctxt;
        return equalRange<Index>(value, ctxt);
    }

    template<typename Index>
    range<typename indices_t::template iter_type<Index>::type>
    equalRange(const tuple_type& value, operation_context& context) const {
        return indices.template equalRange<Index>(value,context);
    }

    template<unsigned ... Columns>
    auto equalRange(const tuple_type& value) const
            -> decltype(this->template equalRange<index<Columns...>>(value)) {
        return equalRange<index<Columns...>>(value);
    }

    template<unsigned ... Columns, typename Context>
    auto equalRange(const tuple_type& value, Context& ctxt) const
            -> decltype(this->template equalRange<index<Columns...>>(value,ctxt)) {
        return equalRange<index<Columns...>>(value,ctxt);
    }

    auto begin() const -> decltype(indices.getIndex(primary_index()).begin()) {
        return indices.getIndex(primary_index()).begin();
    }

    auto end() const -> decltype(indices.getIndex(primary_index()).end()) {
        return indices.getIndex(primary_index()).end();
    }

    void purge() {
        indices.clear();
    }

    auto partition() -> decltype(indices.partition(primary_index())) {
        return indices.partition(primary_index());
    }

    /* Prints a description of the internal structure of this relation. */
    std::ostream& printDescription(std::ostream& out = std::cout) const {
        out << "DirectIndexedRelation of arity=" << arity << " with indices [ ";
        indices.printDescription(out);
        out << " ] where " << primary_index() << " is the primary index";
        return out;
    }

};

// This code enables the configuration of the type of relation for which a direct index should be utilized

// every 2-ary relation shall be directly indexed
template<typename First, typename Second, typename ... Rest>
class Relation<2,First,Second,Rest...> : public DirectIndexedRelation<2,First,Second,Rest...> {};

// every 3-ary relation shall be directly indexed
template<typename First, typename Second, typename ... Rest>
class Relation<3,First,Second,Rest...> : public DirectIndexedRelation<3,First,Second,Rest...> {};

// every 4-ary relation shall be directly indexed
template<typename First, typename Second, typename ... Rest>
class Relation<4,First,Second,Rest...> : public DirectIndexedRelation<4,First,Second,Rest...> {};

// every 5-ary relation shall be directly indexed
template<typename First, typename Second, typename ... Rest>
class Relation<5,First,Second,Rest...> : public DirectIndexedRelation<5,First,Second,Rest...> {};

// every 6-ary relation shall be directly indexed
template<typename First, typename Second, typename ... Rest>
class Relation<6,First,Second,Rest...> : public DirectIndexedRelation<6,First,Second,Rest...> {};


/**
 * A specialization of a relation for which no indices are required.
 * Such a relation is mapped to a relation is mapped to a single-index relation
 * with a full index.
 *
 * TODO: consider using a hash table since no range or equality queries are needed
 */
template<unsigned arity>
class Relation<arity> : public Relation<arity, typename index_utils::get_full_index<arity>::type> {};

/**
 * A specialization of a 0-ary relation.
 */
template<>
struct Relation<0> : public RelationBase<0,Relation<0>> {

    typedef RelationBase<0,Relation<0>> base;

    /* The flag indicating whether the empty tuple () is present or not. */
    bool present;

public:

    /* The type of tuple stored in this relation. */
    typedef typename base::tuple_type tuple_type;

    /* The iterator utilized for iterating over elements of this relation. */
    class iterator : public std::iterator<std::forward_iterator_tag,tuple_type> {

        /* A flag indicating whether this iterator points to the empty tuple or not. */
        bool begin;

    public:

        // (default) constructors
        iterator(bool begin = false) : begin(begin) {}
        iterator(const iterator&) = default;

        // the default assignment operator
        iterator& operator=(const iterator&) = default;

        // the equality operator as required by the iterator concept
        bool operator==(const iterator& other) const {
            return begin == other.begin;
        }

        // the not-equality operator as required by the iterator concept
        bool operator!=(const iterator& other) const {
            return begin != other.begin;
        }

        // the deref operator as required by the iterator concept
        const tuple_type& operator*() const {
            return getSingleton();
        }

        // support for the pointer operator
        const tuple_type* operator->() const {
            return &getSingleton();
        }

        // the increment operator as required by the iterator concept
        iterator& operator++() {
            begin=false;
            return *this;
        }

    private:

    	/* A singleton instance of the empty tuple. */
        static const tuple_type& getSingleton() {
        	static const tuple_type singleton = tuple_type();
        	return singleton;
        }

    };

    /* The operation context for this relation - which is emtpy. */
    struct operation_context {};

    /* A constructor for this relation. */
    Relation() : present(false) {};

    // --- specialized implementation ---

    operation_context createContext() {
        return operation_context();
    }

    bool empty() const {
        return !present;
    }

    std::size_t size() const {
        return (present) ? 1 : 0;
    }

    bool contains(const tuple_type& = tuple_type(), const operation_context& = operation_context()) const {
        return present;
    }

    bool insert(const tuple_type& = tuple_type(), const operation_context& = operation_context()) {
        bool res = !present;
        present = true;
        return res;
    }

    template<typename ... Idxs>
    void insertAll(const Relation<0,Idxs...>& other) {
        present = present || other.present;
    }

    template<typename Index>
    range<iterator> scan() const {
        return make_range(begin(), end());
    }

    template<typename Index>
    range<iterator> equalRange(const tuple_type& value) const {
        return make_range(begin(), end());
    }

    template<typename Index>
    range<iterator> equalRange(const tuple_type& value, operation_context& ctxt) const {
        return make_range(begin(), end());
    }

    template<unsigned ... Columns>
    range<iterator> equalRange(const tuple_type& value) const {
        return equalRange<index<Columns...>>(value);
    }

    template<unsigned ... Columns, typename Context>
    range<iterator> equalRange(const tuple_type& value, Context& ctxt) const {
        return equalRange<index<Columns...>>(value,ctxt);
    }

    iterator begin() const {
        return iterator(present);
    }

    iterator end() const {
        return iterator(false);
    }

    void purge() {
        present = false;
    }

    std::vector<range<iterator>> partition() const {
        return toVector(make_range(begin(), end()));
    }

    /* Prints a description of the internal organization of this relation. */
    std::ostream& printDescription(std::ostream& out = std::cout) const {
        return out << "Nullary Relation";
    }

};


/**
 * A specialization of the relation requesting a single index.
 */
template<unsigned arity, typename Index>
class Relation<arity, Index> : public RelationBase<arity,Relation<arity, Index>> {

    // expand only index to a full index
    typedef typename index_utils::extend_to_full_index<arity,Index>::type primary_index_t;
    static_assert(primary_index_t::size == arity, "Single index is not a full index!");

    // a shortcut for the base class
    typedef RelationBase<arity,Relation<arity, Index>> base;

public:

    /* The tuple type handled by this relation. */
    typedef typename base::tuple_type tuple_type;

private:

    // this variant stores all tuples in the one index
    typedef typename index_utils::direct_index_factory<tuple_type, primary_index_t,true>::type table_t;

    /* The indexed data stored in this relation. */
    table_t data;

public:

    /* The iterator type utilized by this relation. */
    typedef typename table_t::iterator iterator;

    // import generic signatures from the base class
    using base::insert;
    using base::contains;

    typedef typename table_t::operation_hints operation_context;

    // --- most general implementation ---

    operation_context createContext() {
        return operation_context();
    }

    bool empty() const {
        return data.empty();
    }

    std::size_t size() const {
        return data.size();
    }

    bool contains(const tuple_type& tuple, operation_context& ctxt) const {
        return data.contains(tuple, ctxt);
    }

    bool insert(const tuple_type& tuple, operation_context& ctxt) {
        return data.insert(tuple, ctxt);
    }

    void insertAll(const Relation& other) {
        data.insertAll(other.data);
    }

    template<typename ... Idxs>
    void insertAll(const Relation<arity,Idxs...>& other) {
        operation_context ctxt;
        for(const tuple_type& cur : other) {
            insert(cur, ctxt);
        }
    }

    template<typename I>
    range<iterator> scan() const {
        static_assert(std::is_same<Index,I>::value, "Addressing uncovered index!");
        return make_range(data.begin(), data.end());
    }

    template<typename I>
    range<iterator> equalRange(const tuple_type& value) const {
        operation_context ctxt;
        return equalRange<I>(value, ctxt);
    }

    template<typename I>
    range<iterator> equalRange(const tuple_type& value, operation_context& ctxt) const {
        return data.template equalRange<I>(value, ctxt);
    }

    template<unsigned ... Columns>
    range<iterator> equalRange(const tuple_type& value) const {
        return equalRange<index<Columns...>>(value);
    }

    template<unsigned ... Columns, typename Context>
    range<iterator> equalRange(const tuple_type& value, Context& ctxt) const {
        return equalRange<index<Columns...>>(value,ctxt);
    }

    iterator begin() const {
        return data.begin();
    }

    iterator end() const {
        return data.end();
    }

    void purge() {
        data.clear();
    }

    std::vector<range<iterator>> partition() const {
        return data.partition();
    }

    /* Prints a description of the inner organization of this relation. */
    std::ostream& printDescription(std::ostream& out = std::cout) const {
        out << "Index-Organized Relation of arity=" << arity << " based on a ";
        table_t::printDescription(out);
        return out;
    }
};

} // end namespace ram
