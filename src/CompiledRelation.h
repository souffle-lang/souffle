/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CompiledRelation.h
 *
 * The central file covering the data structure utilized by
 * the souffle compiler for representing relations in compiled queries.
 *
 ***********************************************************************/

#pragma once

#include "CompiledIndexUtils.h"
#include "CompiledTuple.h"
#include "ParallelUtils.h"
#include "RamTypes.h"
#include "Table.h"
#include "Util.h"
#include <cassert>
#include <iostream>
#include <iterator>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace souffle {

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
template <typename Setup, unsigned arity, typename... Indices>
class Relation : public Setup::template relation<arity, Indices...> {};

namespace detail {
struct std_set_config;
struct std_unordered_config;
}  // namespace detail

template <typename config>
struct GenericSetup;

/**
 * A generic, tuned setup, using a combination of direct and indirect
 * b-trees, bries and ordinary tables.
 */
struct Auto;

/**
 * A setup utilizing direct b-trees for relations exclusively.
 */
struct BTree;
/**
 * A setup utilizing bries for relations exclusively.
 */
struct Brie;

/**
 * A setup utilizing disjoint set data structures
 */
struct EqRel;

/**
 * A setup utilizing hash based data structures exclusively (STL unordered set and multiset).
 */
using Hashset = GenericSetup<detail::std_unordered_config>;

/**
 * A setup utilizing a std set and multiset based structures exclusively.
 */
using Rbtset = GenericSetup<detail::std_set_config>;

// -------------------------------------------------------------
//                  Auto Setup Implementation
// -------------------------------------------------------------

namespace detail {

/**
 * The relation implementing the type if relation utilizing a mixture of different
 * types of data structures for its representation. The actual data structures
 * are implementation dependent.
 */
template <unsigned arity, typename... Indices>
class AutoRelation;
}  // namespace detail

/**
 * A generic, tuned setup, using a combination of direct and indirect
 * b-trees, bries and ordinary tables. The actual data structures
 * are implementation dependent.
 */
struct Auto {
    // determines the relation implementation for a given use case
    template <unsigned arity, typename... Indices>
    using relation = detail::AutoRelation<arity, Indices...>;
};

// -------------------------------------------------------------
//                  BTree Setup Implementation
// -------------------------------------------------------------

namespace detail {

/**
 * The relation type utilized to implement relations only utilizing a single
 * type of data structure (for the pure BTree and Brie mode).
 */
template <template <typename Tuple, typename Index, bool direct> class IndexFactory, unsigned arity,
        typename... Indices>
class SingleIndexTypeRelation;
}  // namespace detail

/**
 * A setup utilizing direct b-trees for relations exclusively.
 */
struct BTree {
    // a index factory selecting in any case a BTree index
    template <typename Tuple, typename Index, bool>
    struct btree_index_factory {
        using type = typename index_utils::DirectIndex<Tuple, Index>;
    };

    // determines the relation implementation for a given use case
    template <unsigned arity, typename... Indices>
    using relation = detail::SingleIndexTypeRelation<btree_index_factory, arity, Indices...>;
};

// -------------------------------------------------------------
//                  Brie Setup Implementation
// -------------------------------------------------------------

/**
 * A setup utilizing bries for relations exclusively.
 */
struct Brie {
    // a index factory selecting in any case a Brie index
    template <typename Tuple, typename Index, bool>
    struct brie_index_factory {
        using type = typename index_utils::TrieIndex<Index>;
    };

    // determines the relation implementation for a given use case
    template <unsigned arity, typename... Indices>
    using relation = detail::SingleIndexTypeRelation<brie_index_factory, arity, Indices...>;
};

// -------------------------------------------------------------
//                  EqRel Setup Implementation
// -------------------------------------------------------------

/**
 * A setup utilizing disjoint sets for binary relations exclusively
 */
struct EqRel {
    template <typename Tuple, typename Index, bool>
    struct eqrel_index_factory {
        using type = typename index_utils::DisjointSetIndex<Index>;
    };

    // determines the relation implementation for a given use case
    template <unsigned arity, typename... Indices>
    using relation = detail::SingleIndexTypeRelation<eqrel_index_factory, arity, Indices...>;
};

// -------------------------------------------------------------
//                  Generic Setup Implementation
// -------------------------------------------------------------

namespace detail {

/**
 * A generic relation type to customizable by some data structure
 * configuration struct.
 */
template <typename config, unsigned arity, typename... Indices>
class GenericRelation;
}  // namespace detail

/**
 * A setup utilizing a configurable relation type.
 */
template <typename config>
struct GenericSetup {
    // determines the relation implementation for a given use case
    template <unsigned arity, typename... Indices>
    using relation = typename std::conditional<arity == 0, detail::AutoRelation<0>,
            detail::GenericRelation<config, arity, Indices...>>::type;
};

namespace detail {
/**
 * A base class for partially specialized relation templates following below.
 * This base class provides generic interfaces and adapters forwarding requests
 * to the most general version offered by the derived classes to save implementation
 * overhead and to unify the interface.
 *
 * @tparam arity .. the arity of the resulting relation
 * @tparam Derived .. the type of the derived relation
 */
template <unsigned arity, typename Derived>
struct RelationBase {
    // the type of tuple maintained by this relation
    using tuple_type = Tuple<RamDomain, arity>;

    // -- contains wrapper --

    template <typename... Args>
    bool contains(Args... args) const {
        RamDomain data[arity] = {RamDomain(args)...};
        return static_cast<const Derived*>(this)->contains(reinterpret_cast<const tuple_type&>(data));
    }

    bool contains(const tuple_type& tuple) const {
        typename Derived::operation_context ctxt;
        return static_cast<const Derived*>(this)->contains(tuple, ctxt);
    }

    // -- insert wrapper --

    template <typename... Args>
    bool insert(Args... args) {
        RamDomain data[arity] = {RamDomain(args)...};
        return static_cast<Derived*>(this)->insert(reinterpret_cast<const tuple_type&>(data));
    }

    bool insert(const RamDomain* ramDomain) {
        RamDomain data[arity];
        std::copy(ramDomain, ramDomain + arity, data);
        const auto& tuple = reinterpret_cast<const tuple_type&>(data);
        typename Derived::operation_context ctxt;

        return static_cast<Derived*>(this)->insert(tuple, ctxt);
    }

    bool insert(const tuple_type& tuple) {
        typename Derived::operation_context ctxt;
        return static_cast<Derived*>(this)->insert(tuple, ctxt);
    }

    // -- IO --

    /* Provides a description of the internal organization of this relation. */
    std::string getDescription() const {
        std::stringstream out;
        static_cast<const Derived*>(this)->printDescription(out);
        return out.str();
    }

    // -- Hint Profiling --

    /* Prints a summary of the hint statistic of this relation */
    void printHintStatistics(std::ostream& out, const std::string& prefix = "") const {
        static_cast<const Derived*>(this)->printHintStatistics(out, prefix);
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
template <unsigned arity, typename... Indices>
class AutoRelation : public RelationBase<arity, AutoRelation<arity, Indices...>> {
    // check validity of indices
    static_assert(index_utils::check<arity, Indices...>::value, "Warning: invalid indices combination!");

    // shortcut for the base class
    using base = RelationBase<arity, AutoRelation<arity, Indices...>>;

public:
    /* The type of tuple stored in this relation. */
    using tuple_type = typename base::tuple_type;

    /* The table storing the master-copies of the relations. */
    using table_t = Table<tuple_type>;

    /* The iterator type to be utilized for relation scans. */
    using iterator = typename table_t::iterator;

private:
    // obtain type of index collection
    using indices_t = typename std::conditional<
            // check whether there is at least one index covering all columns
            index_utils::contains_full_index<arity, Indices...>::value,
            // if so: just create those indices
            index_utils::Indices<tuple_type, index_utils::index_factory, Indices...>,
            // otherwise: add an additional full index
            index_utils::Indices<tuple_type, index_utils::index_factory,
                    typename index_utils::get_full_index<arity>::type, Indices...>>::type;

    // define the primary index for existence checks
    using primary_index = typename index_utils::get_first_full_index<arity, Indices...,
            typename index_utils::get_full_index<arity>::type>::type;

    // the data stored in this relation (main copy, referenced by indices)
    table_t data;

    // all other indices
    indices_t indices;

    // the lock utilized to synchronize inserts
    Lock insert_lock;

    /* A utility to check whether a certain index is covered by this relation. */
    template <typename Index>
    struct covered {
        enum { value = indices_t::template is_covered<Index>::value };
    };

public:
    /* The context information to be utilized by operations on this relation. */
    using operation_context = typename indices_t::operation_context;

    // import generic signatures from the base class
    using base::contains;
    using base::insert;

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

    template <typename Setup, typename... Idxs>
    void extend(const Relation<Setup, arity, Idxs...>& other) {
        data.extend(other.data);
    }

    template <typename Setup, typename... Idxs>
    void insertAll(const Relation<Setup, arity, Idxs...>& other) {
        operation_context context;
        for (const tuple_type& cur : other) {
            insert(cur, context);
        }
    }

    template <typename Index>
    auto scan() const -> decltype(indices.scan(Index())) {
        return indices.scan(Index());
    }

    // -- equal range wrapper --

    template <typename Index>
    range<typename indices_t::template iter_type<Index>::type> equalRange(const tuple_type& value) const {
        operation_context ctxt;
        return equalRange<Index>(value, ctxt);
    }

    template <typename Index>
    range<typename indices_t::template iter_type<Index>::type> equalRange(
            const tuple_type& value, operation_context& context) const {
        static_assert(covered<Index>::value, "Addressing uncovered index!");
        return indices.template equalRange<Index>(value, context);
    }

    template <unsigned... Columns>
    range<typename indices_t::template iter_type<index<Columns...>>::type> equalRange(
            const tuple_type& value) const {
        return equalRange<index<Columns...>>(value);
    }

    template <unsigned... Columns, typename Context>
    range<typename indices_t::template iter_type<index<Columns...>>::type> equalRange(
            const tuple_type& value, Context& ctxt) const {
        return equalRange<index<Columns...>>(value, ctxt);
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

    /* Prints a summary of the hint statistic of this relation */
    void printHintStatistics(std::ostream& out, const std::string& prefix = "") const {
        indices.printHintStatistics(out, prefix);
    }
};

/**
 * A special relation that only utilizes direct indices.
 *
 * @tparam arity .. the arity of the resulting relation
 * @tparam Indices .. the indices to be maintained on top
 */
template <template <typename V, typename I, bool> class IndexFactory, unsigned arity, typename Primary,
        typename... Indices>
class DirectIndexedRelation
        : public RelationBase<arity, DirectIndexedRelation<IndexFactory, arity, Primary, Indices...>> {
    //    // check validity of indices
    //    static_assert(
    //            index_utils::check<arity,
    //                typename index_utils::extend_to_full_index<arity, Primary>::type,
    //                typename index_utils::extend_to_full_index<arity, Indices>::type...
    //            >::value,
    //            "Warning: invalid indices combination!");

    // shortcut for the base class
    using base = RelationBase<arity, DirectIndexedRelation<IndexFactory, arity, Primary, Indices...>>;

public:
    /* The type of tuple stored in this relation. */
    using tuple_type = typename base::tuple_type;

private:
    // obtain type of index collection
    using indices_t = typename index_utils::Indices<tuple_type, IndexFactory,
            typename index_utils::extend_to_full_index<arity, Primary>::type,
            typename index_utils::extend_to_full_index<arity, Indices>::type...>;

    // define the primary index for existence checks
    using primary_index = typename index_utils::extend_to_full_index<arity, Primary>::type;

    // all other indices
    indices_t indices;

public:
    /* iterator type */
    using iterator = decltype(indices.getIndex(primary_index()).begin());

public:
    /* The context information to be utilized by operations on this relation. */
    using operation_context = typename indices_t::operation_context;

    // import generic signatures from the base class
    using base::contains;
    using base::insert;

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

    template <typename Setup, typename... Idxs>
    void insertAll(const Relation<Setup, arity, Idxs...>& other) {
        operation_context context;
        for (const tuple_type& cur : other) {
            insert(cur, context);
        }
    }

    template <typename Index>
    auto scan() const -> decltype(indices.scan(Index())) {
        return indices.scan(Index());
    }

    template <typename Index>
    range<typename indices_t::template iter_type<Index>::type> equalRange(const tuple_type& value) const {
        operation_context ctxt;
        return equalRange<Index>(value, ctxt);
    }

    template <typename Index>
    range<typename indices_t::template iter_type<Index>::type> equalRange(
            const tuple_type& value, operation_context& context) const {
        return indices.template equalRange<Index>(value, context);
    }

    template <unsigned... Columns>
    auto equalRange(const tuple_type& value) const
            -> decltype(this->template equalRange<index<Columns...>>(value)) {
        return equalRange<index<Columns...>>(value);
    }

    template <unsigned... Columns, typename Context>
    auto equalRange(const tuple_type& value, Context& ctxt) const
            -> decltype(this->template equalRange<index<Columns...>>(value, ctxt)) {
        return equalRange<index<Columns...>>(value, ctxt);
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

    /* Prints a summary of the hint statistic of this relation */
    void printHintStatistics(std::ostream& out, const std::string& prefix = "") const {
        indices.printHintStatistics(out, prefix);
    }
};

// This code enables the configuration of the type of relation for which a direct index should be utilized

// every 2-ary relation shall be directly indexed
template <typename First, typename Second, typename... Rest>
class AutoRelation<2, First, Second, Rest...>
        : public DirectIndexedRelation<index_utils::direct_index_factory, 2, First, Second, Rest...> {};

// every 3-ary relation shall be directly indexed
template <typename First, typename Second, typename... Rest>
class AutoRelation<3, First, Second, Rest...>
        : public DirectIndexedRelation<index_utils::direct_index_factory, 3, First, Second, Rest...> {};

// every 4-ary relation shall be directly indexed
template <typename First, typename Second, typename... Rest>
class AutoRelation<4, First, Second, Rest...>
        : public DirectIndexedRelation<index_utils::direct_index_factory, 4, First, Second, Rest...> {};

// every 5-ary relation shall be directly indexed
template <typename First, typename Second, typename... Rest>
class AutoRelation<5, First, Second, Rest...>
        : public DirectIndexedRelation<index_utils::direct_index_factory, 5, First, Second, Rest...> {};

// every 6-ary relation shall be directly indexed
template <typename First, typename Second, typename... Rest>
class AutoRelation<6, First, Second, Rest...>
        : public DirectIndexedRelation<index_utils::direct_index_factory, 6, First, Second, Rest...> {};

/**
 * A specialization of a relation for which no indices are required.
 * Such a relation is mapped to a relation is mapped to a single-index relation
 * with a full index.
 *
 * TODO: consider using a hash table since no range or equality queries are needed
 */
template <unsigned arity>
class AutoRelation<arity> : public AutoRelation<arity, typename index_utils::get_full_index<arity>::type> {};

/**
 * A specialization of a 0-ary relation.
 */
template <>
class AutoRelation<0> : public RelationBase<0, AutoRelation<0>> {
    using base = RelationBase<0, AutoRelation<0>>;

    /* The flag indicating whether the empty tuple () is present or not. */
    bool present = false;

public:
    /* The type of tuple stored in this relation. */
    using tuple_type = typename base::tuple_type;

    /* The iterator utilized for iterating over elements of this relation. */
    class iterator : public std::iterator<std::forward_iterator_tag, tuple_type> {
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
            begin = false;
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
    AutoRelation() = default;

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

    bool insert(const RamDomain* ramDomain) {
        return insert();
    }

    bool insert(const tuple_type& = tuple_type(), const operation_context& = operation_context()) {
        bool res = !present;
        present = true;
        return res;
    }

    template <typename Setup, typename... Idxs>
    void insertAll(const Relation<Setup, 0, Idxs...>& other) {
        present = present || other.present;
    }

    template <typename Index>
    range<iterator> scan() const {
        static_assert(std::is_same<Index, index<>>::value, "Requesting uncovered index!");
        return make_range(begin(), end());
    }

    template <typename Index>
    range<iterator> equalRange(const tuple_type& value) const {
        static_assert(std::is_same<Index, index<>>::value, "Requesting uncovered index!");
        return make_range(begin(), end());
    }

    template <typename Index>
    range<iterator> equalRange(const tuple_type& value, operation_context& ctxt) const {
        static_assert(std::is_same<Index, index<>>::value, "Requesting uncovered index!");
        return make_range(begin(), end());
    }

    template <unsigned... Columns>
    range<iterator> equalRange(const tuple_type& value) const {
        return equalRange<index<Columns...>>(value);
    }

    template <unsigned... Columns, typename Context>
    range<iterator> equalRange(const tuple_type& value, Context& ctxt) const {
        return equalRange<index<Columns...>>(value, ctxt);
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

    /* Prints a summary of the hint statistic of this relation */
    void printHintStatistics(std::ostream& out, const std::string& prefix = "") const {
        out << prefix << " -- no hints used in nullary relation --\n";
    }
};

/**
 * A specialization of the relation requesting a single index.
 */
template <unsigned arity, typename Index, template <typename T, typename I, bool d> class table_factory>
class SingleIndexRelation : public RelationBase<arity, SingleIndexRelation<arity, Index, table_factory>> {
    // expand only index to a full index
    using primary_index_t = typename index_utils::extend_to_full_index<arity, Index>::type;
    static_assert(primary_index_t::size == arity, "Single index is not a full index!");

    // a shortcut for the base class
    using base = RelationBase<arity, SingleIndexRelation<arity, Index, table_factory>>;

public:
    /* The tuple type handled by this relation. */
    using tuple_type = typename base::tuple_type;

private:
    // this variant stores all tuples in the one index
    using table_t = typename table_factory<tuple_type, primary_index_t, true>::type;

    /* The indexed data stored in this relation. */
    table_t data;

public:
    /* The iterator type utilized by this relation. */
    using iterator = typename table_t::iterator;

    // import generic signatures from the base class
    using base::contains;
    using base::insert;

    using operation_context = typename table_t::operation_hints;

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

    const table_t& getData() const {
        return data;
    }

    bool contains(const tuple_type& tuple, operation_context& ctxt) const {
        return data.contains(tuple, ctxt);
    }

    bool insert(const tuple_type& tuple, operation_context& ctxt) {
        return data.insert(tuple, ctxt);
    }

    void insertAll(const SingleIndexRelation& other) {
        data.insertAll(other.data);
    }

    /** Extend this relation with the knowledge created by inserting into other. */
    template <typename Setup, typename... Idxs>
    void extend(const Relation<Setup, arity, Idxs...>& other) {
        data.extend(other.getData());
    }

    template <typename... Idxs>
    void insertAll(const Relation<souffle::ram::EqRel, arity, Idxs...>& other) {
        data.insertAll(other.getData());
    }

    template <typename Setup, typename... Idxs>
    void insertAll(const Relation<Setup, arity, Idxs...>& other) {
        operation_context ctxt;
        for (const tuple_type& cur : other) {
            insert(cur, ctxt);
        }
    }

    template <typename I>
    range<iterator> scan() const {
        static_assert(index_utils::is_compatible_with<I, Index>::value, "Addressing uncovered index!");
        return make_range(data.begin(), data.end());
    }

private:
    template <typename I>
    typename std::enable_if<index_utils::is_compatible_with<I, Index>::value, range<iterator>>::type
    equalRangeInternal(const tuple_type& value, operation_context& ctxt) const {
        return data.template equalRange<I>(value, ctxt);
    }

    template <typename I>
    typename std::enable_if<!index_utils::is_compatible_with<I, Index>::value,
            range<iterator_utils::filter_iterator<iterator, I>>>::type
    equalRangeInternal(const tuple_type& value, operation_context&) const {
        return make_range(iterator_utils::filter_iterator<iterator, I>(begin(), end(), value),
                iterator_utils::filter_iterator<iterator, I>(end(), end(), value));
    }

public:
    template <typename I>
    auto equalRange(const tuple_type& value, operation_context& ctxt) const
            -> decltype(this->equalRangeInternal<I>(value, ctxt)) {
        return equalRangeInternal<I>(value, ctxt);
    }

    template <typename I>
    auto equalRange(const tuple_type& value) const
            -> decltype(this->equalRangeInternal<I>(value, std::declval<operation_context&>())) {
        operation_context ctxt;
        return equalRange<I>(value, ctxt);
    }

    template <unsigned... Columns>
    auto equalRange(const tuple_type& value) const -> decltype(
            this->equalRangeInternal<index<Columns...>>(value, std::declval<operation_context&>())) {
        return equalRange<index<Columns...>>(value);
    }

    template <unsigned... Columns, typename Context>
    auto equalRange(const tuple_type& value, Context& ctxt) const
            -> decltype(this->equalRangeInternal<index<Columns...>>(value, ctxt)) {
        return equalRange<index<Columns...>>(value, ctxt);
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

    /* Prints a summary of the hint statistic of this relation */
    void printHintStatistics(std::ostream& out, const std::string& prefix = "") const {
        data.printHintStatistics(out, prefix);
    }
};

/**
 * A specialization of the relation requesting a single index.
 */
template <unsigned arity, typename Index>
class AutoRelation<arity, Index>
        : public SingleIndexRelation<arity, Index, index_utils::direct_index_factory> {};

// ------------------------------------------------------------------------------------------
//                              SingleIndexTypeRelation
// ------------------------------------------------------------------------------------------

/**
 * The implementation of a relation using the same kind of index for all its internally
 * maintained data structures.
 */
template <template <typename Tuple, typename Index, bool direct> class IndexFactory, unsigned arity,
        typename... Indices>
class SingleIndexTypeRelation : public DirectIndexedRelation<IndexFactory, arity, Indices...> {};

/**
 * A specialization of the single index type relation if there is only a single index
 * required.
 */
template <template <typename Tuple, typename Index, bool direct> class IndexFactory, unsigned arity,
        typename Index>
class SingleIndexTypeRelation<IndexFactory, arity, Index>
        : public SingleIndexRelation<arity, Index, IndexFactory> {};

/**
 * A specialization of the single index type relation for the case that there
 * is no index required. In this case, we treat it like a single, full index.
 */
template <template <typename Tuple, typename Index, bool direct> class IndexFactory, unsigned arity>
class SingleIndexTypeRelation<IndexFactory, arity>
        : public SingleIndexTypeRelation<IndexFactory, arity,
                  typename index_utils::get_full_index<arity>::type> {};

/**
 * A specialization for the case of a 0-arity relation. In this case, we will reuse
 * the default, since creating any data structure is a weast of resources.
 */
template <template <typename Tuple, typename Index, bool direct> class IndexFactory>
class SingleIndexTypeRelation<IndexFactory, 0> : public AutoRelation<0> {};

// ------------------------------------------------------------------------------------------
//                                     GenericRelation
// ------------------------------------------------------------------------------------------

template <typename config, unsigned arity, typename... Indices>
class GenericRelationGroup;

template <typename config, unsigned arity>
class GenericRelationGroup<config, arity> {
    using tuple_type = Tuple<RamDomain, arity>;

public:
    template <typename Index>
    void get() {}

    template <typename Index>
    void get() const {}

    template <typename Index>
    void equal_range(const tuple_type&) const {}

    void insert(const tuple_type&) {
        // nothing to do here
    }

    void clear() {
        // nothing to do here
    }

    range<tuple_type*> equal_range(const tuple_type&) const {
        assert(false && "Missing index!");
        return {nullptr, nullptr};
    }
};

template <typename config, unsigned arity, typename First, typename... Rest>
class GenericRelationGroup<config, arity, First, Rest...> {
    using this_type = GenericRelationGroup<config, arity, First, Rest...>;

    using tuple_type = Tuple<RamDomain, arity>;

    using data_type = typename config::template set_type<arity, First>;

    using nested_group = GenericRelationGroup<config, arity, Rest...>;

    using iterator = typename data_type::const_iterator;

    // the storage of this index
    data_type data;

    // the remaining indices
    nested_group nested;

public:
    template <typename Index>
    typename std::enable_if<config::template covers_query<Index, First>::value, this_type&>::type get() {
        return *this;
    }

    template <typename Index>
    const typename std::enable_if<config::template covers_query<Index, First>::value, this_type>::type& get()
            const {
        return *this;
    }

    template <typename Index>
    typename std::enable_if<!config::template covers_query<Index, First>::value,
            typename std::remove_reference<decltype(nested.template get<Index>())>::type>::type&
    get() {
        return nested.template get<Index>();
    }

    template <typename Index>
    const typename std::enable_if<!config::template covers_query<Index, First>::value,
            typename std::remove_reference<decltype(nested.template get<Index>())>::type>::type&
    get() const {
        return nested.template get<Index>();
    }

    /*
    template <typename Index>
    typename std::enable_if<config::template covers_query<Index, First>::value, range<iterator>>::type
    equal_range(const tuple_type& t) const {
        return make_range(data.lower_bound(index_utils::lower<First, Index>(t)),
                data.upper_bound(index_utils::raise<First, Index>(t)));
    }*/

    template <typename Index>
    typename std::enable_if<config::template covers_query<Index, First>::value &&
                                    config::template use_equal_range<Index, First>::value,
            range<iterator>>::type
    equal_range(const tuple_type& t) const {
        // get lower and upper bounds in one step
        auto pair = data.equal_range(t);
        return make_range(pair.first, pair.second);
    }

    template <typename Index>
    typename std::enable_if<config::template covers_query<Index, First>::value &&
                                    !config::template use_equal_range<Index, First>::value,
            range<iterator>>::type
    equal_range(const tuple_type& t) const {
        // compute lower and upper bounds in two steps
        return make_range(data.lower_bound(index_utils::lower<First, Index>(t)),
                data.upper_bound(index_utils::raise<First, Index>(t)));
    }

    template <typename Index>
    typename std::enable_if<!config::template covers_query<Index, First>::value,
            decltype(nested.template equal_range<Index>(tuple_type()))>::type
    equal_range(const tuple_type& t) const {
        return nested.template equal_range<Index>(t);
    }

    bool empty() const {
        return data.empty();
    }

    std::size_t size() const {
        return data.size();
    }

    bool contains(const tuple_type& element) const {
        return data.find(element) != data.end();
    }

    void insert(const tuple_type& element) {
        data.insert(element);
        nested.insert(element);
    }

    void clear() {
        data.clear();
        nested.clear();
    }

    iterator begin() const {
        return data.begin();
    }

    iterator end() const {
        return data.end();
    }
};

/**
 * The relation type utilized to implement relations only utilizing a hash
 * based data structure.
 */
template <typename config, unsigned arity, typename... Indices>
class GenericRelation : public RelationBase<arity, GenericRelation<config, arity, Indices...>> {
    using base = RelationBase<arity, GenericRelation<config, arity, Indices...>>;

    using tuple_type = typename base::tuple_type;

    // the indices group type
    using group_type = typename std::conditional<index_utils::contains_full_index<arity, Indices...>::value,
            GenericRelationGroup<config, arity, Indices...>,
            GenericRelationGroup<config, arity, typename index_utils::get_full_index<arity>::type,
                    Indices...>>::type;

    // the full index to be utilized
    using full_index = typename index_utils::get_first_full_index<arity, Indices...,
            typename index_utils::get_full_index<arity>::type>::type;

    using main_index_type = typename std::remove_reference<decltype(
            std::declval<group_type>().template get<full_index>())>::type;

    // the set of indices
    group_type indices;

    // a lock to synchronize insertions
    std::mutex lock;

public:
    using iterator = decltype(std::declval<group_type>().template get<full_index>().begin());

    // import generic signatures from the base class
    using base::contains;
    using base::insert;

    // the empty operation context (no data needed)
    struct operation_context {};

    // -- interface implementation --

    operation_context createContext() {
        return {};
    }

    bool empty() const {
        return getMainIndex().empty();
    }

    std::size_t size() const {
        return getMainIndex().size();
    }

    bool contains(const tuple_type& tuple, operation_context&) const {
        return getMainIndex().contains(tuple);
    }

    bool insert(const tuple_type& tuple, operation_context&) {
        std::lock_guard<std::mutex> guard(lock);
        if (contains(tuple)) return false;
        indices.insert(tuple);
        return true;
    }

    template <typename Setup, typename... Idxs>
    void insertAll(const Relation<Setup, arity, Idxs...>& other) {
        for (const tuple_type& cur : other) {
            insert(cur);
        }
    }

    template <typename I>
    range<iterator> scan() const {
        const auto& index = getMainIndex();
        return {index.begin(), index.end()};
    }

    template <typename I>
    auto equalRange(const tuple_type& value, operation_context&) const
            -> decltype(indices.template equal_range<I>(value)) {
        return indices.template equal_range<I>(value);
    }

    template <typename I>
    auto equalRange(const tuple_type& value) const -> decltype(indices.template equal_range<I>(value)) {
        return indices.template equal_range<I>(value);
    }

    template <unsigned... Columns>
    auto equalRange(const tuple_type& value) const
            -> decltype(indices.template equal_range<index<Columns...>>(value)) {
        return indices.template equal_range<index<Columns...>>(value);
    }

    template <unsigned... Columns>
    auto equalRange(const tuple_type& value, operation_context&) const
            -> decltype(indices.template equal_range<index<Columns...>>(value)) {
        return indices.template equal_range<index<Columns...>>(value);
    }

    iterator begin() const {
        return getMainIndex().begin();
    }

    iterator end() const {
        return getMainIndex().end();
    }

    void purge() {
        indices.clear();
    }

    std::vector<range<iterator>> partition() const {
        range<iterator> full(begin(), end());
        return full.partition(100);
    }

    /* Prints a description of the inner organization of this relation. */
    std::ostream& printDescription(std::ostream& out = std::cout) const {
        out << config::getName() << "-based relaton of arity=" << arity;
        return out;
    }

private:
    main_index_type& getMainIndex() {
        return indices.template get<full_index>();
    }

    const main_index_type& getMainIndex() const {
        return indices.template get<full_index>();
    }
};

// ------------------------------------------------------------------------------------------
//                                     StdOrderedRelation
// ------------------------------------------------------------------------------------------

template <typename Index>
struct tuple_less;

template <>
struct tuple_less<index<>> {
    template <std::size_t S>
    bool operator()(const Tuple<RamDomain, S>&, const Tuple<RamDomain, S>&) const {
        return false;
    }
};

template <unsigned First, unsigned... Rest>
struct tuple_less<index<First, Rest...>> {
    template <std::size_t S>
    bool operator()(const Tuple<RamDomain, S>& a, const Tuple<RamDomain, S>& b) const {
        return a[First] < b[First] || (a[First] == b[First] && tuple_less<index<Rest...>>()(a, b));
    }
};

struct std_set_config {
    template <unsigned arity, typename Index>
    using set_type = typename std::conditional<index_utils::is_full_index<arity, Index>::value,
            std::set<Tuple<RamDomain, arity>, tuple_less<Index>>,
            std::multiset<Tuple<RamDomain, arity>, tuple_less<Index>>>::type;

    template <typename Query, typename Index>
    using covers_query = index_utils::is_compatible_with<Query, Index>;

    template <typename Query, typename Index>
    using use_equal_range = std::false_type;  // index_utils::is_permutation<Query,Index>;

    const char* getName() {
        return "std::set";
    }
};

// ------------------------------------------------------------------------------------------
//                                  StdUnorderedRelation
// ------------------------------------------------------------------------------------------

template <typename Index>
struct tuple_hasher;

template <unsigned Pos>
struct tuple_hasher<index<Pos>> {
    template <std::size_t S>
    std::size_t operator()(const Tuple<RamDomain, S>& a) const {
        return a[Pos];
    }
};

template <unsigned First, unsigned... Rest>
struct tuple_hasher<index<First, Rest...>> {
    template <std::size_t S>
    std::size_t operator()(const Tuple<RamDomain, S>& a) const {
        auto h = tuple_hasher<index<Rest...>>()(a);
        return a[First] + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
};

template <typename Index>
struct tuple_equal;

template <>
struct tuple_equal<index<>> {
    template <std::size_t S>
    bool operator()(const Tuple<RamDomain, S>&, const Tuple<RamDomain, S>&) const {
        return true;
    }
};

template <unsigned First, unsigned... Rest>
struct tuple_equal<index<First, Rest...>> {
    template <std::size_t S>
    bool operator()(const Tuple<RamDomain, S>& a, const Tuple<RamDomain, S>& b) const {
        return a[First] == b[First] && tuple_equal<index<Rest...>>()(a, b);
    }
};

struct std_unordered_config {
    template <unsigned arity, typename Index>
    using set_type = typename std::conditional<index_utils::is_full_index<arity, Index>::value,
            std::unordered_set<Tuple<RamDomain, arity>, tuple_hasher<Index>, tuple_equal<Index>>,
            std::unordered_multiset<Tuple<RamDomain, arity>, tuple_hasher<Index>, tuple_equal<Index>>>::type;

    template <typename Query, typename Index>
    using covers_query = index_utils::is_permutation<Query, Index>;

    template <typename Query, typename Index>
    using use_equal_range = std::true_type;

    const char* getName() {
        return "std::unordered_set";
    }
};

}  // end of namespace detail

}  // end of namespace ram
}  // end of namespace souffle
