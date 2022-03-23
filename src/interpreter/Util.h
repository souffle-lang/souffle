/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Util.h
 *
 * @brief Interpreter Utilities.
 ***********************************************************************/

#pragma once

#include "Global.h"
#include "souffle/RamTypes.h"
#include "souffle/datastructure/BTree.h"
#include "souffle/datastructure/BTreeDelete.h"
#include "souffle/datastructure/Brie.h"
#include "souffle/datastructure/EquivalenceRelation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"

namespace souffle::interpreter {
// clang-format off

#define FOR_EACH_PROVENANCE(func, ...) \
    func(Provenance, 2, __VA_ARGS__)   \
    func(Provenance, 3, __VA_ARGS__)   \
    func(Provenance, 4, __VA_ARGS__)   \
    func(Provenance, 5, __VA_ARGS__)   \
    func(Provenance, 6, __VA_ARGS__)   \
    func(Provenance, 7, __VA_ARGS__)   \
    func(Provenance, 8, __VA_ARGS__)   \
    func(Provenance, 9, __VA_ARGS__)   \
    func(Provenance, 10, __VA_ARGS__)  \
    func(Provenance, 11, __VA_ARGS__)  \
    func(Provenance, 12, __VA_ARGS__)  \
    func(Provenance, 13, __VA_ARGS__)  \
    func(Provenance, 14, __VA_ARGS__)  \
    func(Provenance, 15, __VA_ARGS__)  \
    func(Provenance, 16, __VA_ARGS__)  \
    func(Provenance, 17, __VA_ARGS__)  \
    func(Provenance, 18, __VA_ARGS__)  \
    func(Provenance, 19, __VA_ARGS__)  \
    func(Provenance, 20, __VA_ARGS__)  \
    func(Provenance, 21, __VA_ARGS__)  \
    func(Provenance, 22, __VA_ARGS__)  \
    func(Provenance, 23, __VA_ARGS__)  \
    func(Provenance, 24, __VA_ARGS__)  \
    func(Provenance, 25, __VA_ARGS__)  \
    func(Provenance, 26, __VA_ARGS__)  \
    func(Provenance, 27, __VA_ARGS__)  \
    func(Provenance, 28, __VA_ARGS__)  \
    func(Provenance, 29, __VA_ARGS__)  \
    func(Provenance, 30, __VA_ARGS__)

#define FOR_EACH_INCREMENTAL(func, ...) \
    func(Incremental, 2, __VA_ARGS__)   \
    func(Incremental, 3, __VA_ARGS__)   \
    func(Incremental, 4, __VA_ARGS__)   \
    func(Incremental, 5, __VA_ARGS__)   \
    func(Incremental, 6, __VA_ARGS__)   \
    func(Incremental, 7, __VA_ARGS__)   \
    func(Incremental, 8, __VA_ARGS__)   \
    func(Incremental, 9, __VA_ARGS__)   \
    func(Incremental, 10, __VA_ARGS__)  \
    func(Incremental, 11, __VA_ARGS__)  \
    func(Incremental, 12, __VA_ARGS__)  \
    func(Incremental, 13, __VA_ARGS__)  \
    func(Incremental, 14, __VA_ARGS__)  \
    func(Incremental, 15, __VA_ARGS__)  \
    func(Incremental, 16, __VA_ARGS__)  \
    func(Incremental, 17, __VA_ARGS__)  \
    func(Incremental, 18, __VA_ARGS__)  \
    func(Incremental, 19, __VA_ARGS__)  \
    func(Incremental, 20, __VA_ARGS__)  \
    func(Incremental, 21, __VA_ARGS__)  \
    func(Incremental, 22, __VA_ARGS__)  \
    func(Incremental, 23, __VA_ARGS__)  \
    func(Incremental, 24, __VA_ARGS__)  \
    func(Incremental, 25, __VA_ARGS__)  \
    func(Incremental, 26, __VA_ARGS__)  \
    func(Incremental, 27, __VA_ARGS__)  \
    func(Incremental, 28, __VA_ARGS__)  \
    func(Incremental, 29, __VA_ARGS__)  \
    func(Incremental, 30, __VA_ARGS__)

#define FOR_EACH_BTREE(func, ...)\
    func(Btree, 0, __VA_ARGS__) \
    func(Btree, 1, __VA_ARGS__) \
    func(Btree, 2, __VA_ARGS__) \
    func(Btree, 3, __VA_ARGS__) \
    func(Btree, 4, __VA_ARGS__) \
    func(Btree, 5, __VA_ARGS__) \
    func(Btree, 6, __VA_ARGS__) \
    func(Btree, 7, __VA_ARGS__) \
    func(Btree, 8, __VA_ARGS__) \
    func(Btree, 9, __VA_ARGS__) \
    func(Btree, 10, __VA_ARGS__) \
    func(Btree, 11, __VA_ARGS__) \
    func(Btree, 12, __VA_ARGS__) \
    func(Btree, 13, __VA_ARGS__) \
    func(Btree, 14, __VA_ARGS__) \
    func(Btree, 15, __VA_ARGS__) \
    func(Btree, 16, __VA_ARGS__) \
    func(Btree, 17, __VA_ARGS__) \
    func(Btree, 18, __VA_ARGS__) \
    func(Btree, 19, __VA_ARGS__) \
    func(Btree, 20, __VA_ARGS__)

#define FOR_EACH_BTREE_DELETE(func, ...)\
    func(BtreeDelete, 1, __VA_ARGS__) \
    func(BtreeDelete, 2, __VA_ARGS__) \
    func(BtreeDelete, 3, __VA_ARGS__) \
    func(BtreeDelete, 4, __VA_ARGS__) \
    func(BtreeDelete, 5, __VA_ARGS__) \
    func(BtreeDelete, 6, __VA_ARGS__) \
    func(BtreeDelete, 7, __VA_ARGS__) \
    func(BtreeDelete, 8, __VA_ARGS__) \
    func(BtreeDelete, 9, __VA_ARGS__) \
    func(BtreeDelete, 10, __VA_ARGS__) \
    func(BtreeDelete, 11, __VA_ARGS__) \
    func(BtreeDelete, 12, __VA_ARGS__) \
    func(BtreeDelete, 13, __VA_ARGS__) \
    func(BtreeDelete, 14, __VA_ARGS__) \
    func(BtreeDelete, 15, __VA_ARGS__) \
    func(BtreeDelete, 16, __VA_ARGS__) \
    func(BtreeDelete, 17, __VA_ARGS__) \
    func(BtreeDelete, 18, __VA_ARGS__) \
    func(BtreeDelete, 19, __VA_ARGS__) \
    func(BtreeDelete, 20, __VA_ARGS__)

// Brie is disabled for now.
#define FOR_EACH_BRIE(func, ...)
    /* func(Brie, 0, __VA_ARGS__) \ */
    /* func(Brie, 1, __VA_ARGS__) \ */
    /* func(Brie, 2, __VA_ARGS__) \ */
    /* func(Brie, 3, __VA_ARGS__) \ */
    /* func(Brie, 4, __VA_ARGS__) \ */
    /* func(Brie, 5, __VA_ARGS__) \ */
    /* func(Brie, 6, __VA_ARGS__) \ */
    /* func(Brie, 7, __VA_ARGS__) \ */
    /* func(Brie, 8, __VA_ARGS__) \ */
    /* func(Brie, 9, __VA_ARGS__) \ */
    /* func(Brie, 10, __VA_ARGS__) \ */
    /* func(Brie, 11, __VA_ARGS__) \ */
    /* func(Brie, 12, __VA_ARGS__) \ */
    /* func(Brie, 13, __VA_ARGS__) \ */
    /* func(Brie, 14, __VA_ARGS__) \ */
    /* func(Brie, 15, __VA_ARGS__) \ */
    /* func(Brie, 16, __VA_ARGS__) \ */
    /* func(Brie, 17, __VA_ARGS__) \ */
    /* func(Brie, 18, __VA_ARGS__) \ */
    /* func(Brie, 19, __VA_ARGS__) \ */
    /* func(Brie, 20, __VA_ARGS__)   */

#define FOR_EACH_EQREL(func, ...)\
    func(Eqrel, 2, __VA_ARGS__)

#define FOR_EACH(func, ...)                 \
    FOR_EACH_BTREE(func, __VA_ARGS__)       \
    FOR_EACH_BTREE_DELETE(func, __VA_ARGS__)       \
    FOR_EACH_BRIE(func, __VA_ARGS__)        \
    FOR_EACH_INCREMENTAL(func, __VA_ARGS__)  \
    FOR_EACH_PROVENANCE(func, __VA_ARGS__)  \
    FOR_EACH_EQREL(func, __VA_ARGS__)

// clang-format on

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
    int operator()(const T&, const T&) const {
        return 0;
    }
    template <typename T>
    bool less(const T&, const T&) const {
        return false;
    }
    template <typename T>
    bool equal(const T&, const T&) const {
        return true;
    }
};

}  // namespace index_utils

/**
 * The index class is utilized as a template-meta-programming structure
 * to specify and realize indices.
 *
 * @tparam Columns ... the order in which elements of the relation to be indexed
 * 				shell be considered by this index.
 */
template <unsigned... Columns>
struct index {
    // the comparator associated to this index
    using comparator = index_utils::comparator<Columns...>;
};

/**
 * A namespace enclosing utilities required relations to handle indices.
 */
namespace index_utils {

// -- a utility extending a given index by another column --
//   e.g. index<1,0>   =>    index<1,0,2>

template <typename Index, unsigned column>
struct extend;

template <unsigned... Columns, unsigned Col>
struct extend<index<Columns...>, Col> {
    using type = index<Columns..., Col>;
};

// -- obtains a full index for a given arity --

template <unsigned arity>
struct get_full_index {
    using type = typename extend<typename get_full_index<arity - 1>::type, arity - 1>::type;
};

template <>
struct get_full_index<0> {
    using type = index<>;
};

// -- obtains a full provenance index for a given arity --
template <unsigned arity>
struct get_full_prov_index {
    using type = typename extend<typename extend<typename get_full_index<arity - 2>::type, arity - 1>::type,
            arity - 2>::type;
};

// -- obtains a full incremental index for a given arity --
template <unsigned arity>
struct get_full_incremental_index {
    using type = typename extend<typename get_full_index<arity - 1>::type, arity - 1>::type;
};

}  // namespace index_utils

template <std::size_t Arity>
using t_tuple = typename souffle::Tuple<RamDomain, Arity>;

// The comparator to be used for B-tree nodes.
template <std::size_t Arity>
using comparator = typename index_utils::get_full_index<Arity>::type::comparator;

template <std::size_t Arity>
using prov_comparator = typename index_utils::get_full_prov_index<Arity>::type::comparator;

template <std::size_t Arity>
using incremental_comparator = typename index_utils::get_full_incremental_index<Arity>::type::comparator;

// Alias for btree_set
template <std::size_t Arity>
using Btree = btree_set<t_tuple<Arity>, comparator<Arity>>;

// Alias for btree_set
template <std::size_t Arity>
using BtreeDelete = btree_delete_set<t_tuple<Arity>, comparator<Arity>>;

// Alias for Trie
template <std::size_t Arity>
using Brie = Trie<Arity>;

// Updater for Provenance
template <std::size_t Arity>
struct ProvenanceUpdater {
    void update(t_tuple<Arity>& old_t, const t_tuple<Arity>& new_t) {
        old_t[Arity - 2] = new_t[Arity - 2];
        old_t[Arity - 1] = new_t[Arity - 1];
    }
};

// Updater for Provenance
template <std::size_t Arity>
struct IncrementalUpdater {
    void update(t_tuple<Arity>& old_t, const t_tuple<Arity>& new_t) {
        if (new_t[Arity - 1] == 0) {
            old_t[Arity - 1] = 0;
        }
        old_t[Arity - 1] += new_t[Arity - 1];
    }
};

// Alias for Provenance
template <std::size_t Arity>
using Provenance = btree_set<t_tuple<Arity>, prov_comparator<Arity>, std::allocator<t_tuple<Arity>>, 256,
        typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - 2>,
        ProvenanceUpdater<Arity>>;

// Alias for Incremental
template <std::size_t Arity>
using Incremental = btree_set<t_tuple<Arity>, incremental_comparator<Arity>, std::allocator<t_tuple<Arity>>, 256,
        typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - 1>,
        IncrementalUpdater<Arity>>;

// Alias for Eqrel
// Note: require Arity = 2.
template <std::size_t Arity>
using Eqrel = EquivalenceRelation<t_tuple<Arity>>;

};  // namespace souffle::interpreter
