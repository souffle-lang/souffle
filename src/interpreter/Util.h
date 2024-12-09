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
    func(Provenance, 2, 2, __VA_ARGS__)   \
    func(Provenance, 3, 2, __VA_ARGS__)   \
    func(Provenance, 4, 2, __VA_ARGS__)   \
    func(Provenance, 5, 2, __VA_ARGS__)   \
    func(Provenance, 6, 2, __VA_ARGS__)   \
    func(Provenance, 7, 2, __VA_ARGS__)   \
    func(Provenance, 8, 2, __VA_ARGS__)   \
    func(Provenance, 9, 2, __VA_ARGS__)   \
    func(Provenance, 10, 2, __VA_ARGS__)  \
    func(Provenance, 11, 2, __VA_ARGS__)  \
    func(Provenance, 12, 2, __VA_ARGS__)  \
    func(Provenance, 13, 2, __VA_ARGS__)  \
    func(Provenance, 14, 2, __VA_ARGS__)  \
    func(Provenance, 15, 2, __VA_ARGS__)  \
    func(Provenance, 16, 2, __VA_ARGS__)  \
    func(Provenance, 17, 2, __VA_ARGS__)  \
    func(Provenance, 18, 2, __VA_ARGS__)  \
    func(Provenance, 19, 2, __VA_ARGS__)  \
    func(Provenance, 20, 2, __VA_ARGS__)  \
    func(Provenance, 21, 2, __VA_ARGS__)  \
    func(Provenance, 22, 2, __VA_ARGS__)

#define FOR_EACH_BTREE(func, ...)\
    func(Btree, 0, 0, __VA_ARGS__) \
    func(Btree, 1, 0, __VA_ARGS__) \
    func(Btree, 2, 0, __VA_ARGS__) \
    func(Btree, 3, 0, __VA_ARGS__) \
    func(Btree, 4, 0, __VA_ARGS__) \
    func(Btree, 5, 0, __VA_ARGS__) \
    func(Btree, 6, 0, __VA_ARGS__) \
    func(Btree, 7, 0, __VA_ARGS__) \
    func(Btree, 8, 0, __VA_ARGS__) \
    func(Btree, 9, 0, __VA_ARGS__) \
    func(Btree, 10, 0, __VA_ARGS__) \
    func(Btree, 11, 0, __VA_ARGS__) \
    func(Btree, 12, 0, __VA_ARGS__) \
    func(Btree, 13, 0, __VA_ARGS__) \
    func(Btree, 14, 0, __VA_ARGS__) \
    func(Btree, 15, 0, __VA_ARGS__) \
    func(Btree, 16, 0, __VA_ARGS__) \
    func(Btree, 17, 0, __VA_ARGS__) \
    func(Btree, 18, 0, __VA_ARGS__) \
    func(Btree, 19, 0, __VA_ARGS__) \
    func(Btree, 20, 0, __VA_ARGS__) \
    func(Btree, 21, 0, __VA_ARGS__) \
    func(Btree, 22, 0, __VA_ARGS__) \
    func(Btree, 1, 1, __VA_ARGS__)  \
    func(Btree, 2, 1, __VA_ARGS__)  \
    func(Btree, 2, 2, __VA_ARGS__)  \
    func(Btree, 3, 1, __VA_ARGS__)  \
    func(Btree, 3, 2, __VA_ARGS__)  \
    func(Btree, 4, 1, __VA_ARGS__)  \
    func(Btree, 4, 2, __VA_ARGS__)  \
    func(Btree, 5, 1, __VA_ARGS__)  \
    func(Btree, 5, 2, __VA_ARGS__)  \
    func(Btree, 6, 1, __VA_ARGS__)  \
    func(Btree, 6, 2, __VA_ARGS__)  \
    func(Btree, 7, 1, __VA_ARGS__)  \
    func(Btree, 7, 2, __VA_ARGS__)  \
    func(Btree, 8, 1, __VA_ARGS__)  \
    func(Btree, 8, 2, __VA_ARGS__)  \
    func(Btree, 9, 1, __VA_ARGS__)  \
    func(Btree, 9, 2, __VA_ARGS__)

#define FOR_EACH_BTREE_DELETE(func, ...)\
    func(BtreeDelete, 1, 0, __VA_ARGS__) \
    func(BtreeDelete, 2, 0, __VA_ARGS__) \
    func(BtreeDelete, 3, 0, __VA_ARGS__) \
    func(BtreeDelete, 4, 0, __VA_ARGS__) \
    func(BtreeDelete, 5, 0, __VA_ARGS__) \
    func(BtreeDelete, 6, 0, __VA_ARGS__) \
    func(BtreeDelete, 7, 0, __VA_ARGS__) \
    func(BtreeDelete, 8, 0, __VA_ARGS__) \
    func(BtreeDelete, 9, 0, __VA_ARGS__) \
    func(BtreeDelete, 10, 0, __VA_ARGS__) \
    func(BtreeDelete, 11, 0, __VA_ARGS__) \
    func(BtreeDelete, 12, 0, __VA_ARGS__) \
    func(BtreeDelete, 13, 0, __VA_ARGS__) \
    func(BtreeDelete, 14, 0, __VA_ARGS__) \
    func(BtreeDelete, 15, 0, __VA_ARGS__) \
    func(BtreeDelete, 16, 0, __VA_ARGS__) \
    func(BtreeDelete, 17, 0, __VA_ARGS__) \
    func(BtreeDelete, 18, 0, __VA_ARGS__) \
    func(BtreeDelete, 19, 0, __VA_ARGS__) \
    func(BtreeDelete, 20, 0, __VA_ARGS__) \
    func(BtreeDelete, 21, 0, __VA_ARGS__) \
    func(BtreeDelete, 22, 0, __VA_ARGS__)

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
    func(Eqrel, 2, 0, __VA_ARGS__)

#define FOR_EACH(func, ...)                 \
    FOR_EACH_BTREE(func, __VA_ARGS__)       \
    FOR_EACH_BTREE_DELETE(func, __VA_ARGS__)\
    FOR_EACH_BRIE(func, __VA_ARGS__)        \
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

}  // namespace index_utils

template <std::size_t Arity>
using t_tuple = typename souffle::Tuple<RamDomain, Arity>;

// The comparator to be used for B-tree nodes.
template <std::size_t Arity>
using comparator = typename index_utils::get_full_index<Arity>::type::comparator;

// Updater
template <std::size_t Arity, std::size_t AuxiliaryArity>
struct Updater {
    bool update(t_tuple<Arity>& old_t, const t_tuple<Arity>& new_t) {
        bool changed = false;
        // todo: expand for loop using templates
        constexpr std::size_t firstAux = Arity - AuxiliaryArity - 1;
        for (std::size_t i = firstAux; i < Arity; i++) {
            if (old_t[i] != new_t[i]) {
                changed = true;
                old_t[i] = new_t[i];
            }
        }
        return changed;
    }
};

// ProvenanceUpdater
template <std::size_t Arity, std::size_t AuxiliaryArity>
struct ProvenanceUpdater {
    bool update(t_tuple<Arity>& old_t, const t_tuple<Arity>& new_t) {
        constexpr std::size_t level = Arity - 1;
        constexpr std::size_t rule = Arity - 2;
        if (ramBitCast<RamSigned>(new_t[level]) < ramBitCast<RamSigned>(old_t[level]) ||
                (ramBitCast<RamSigned>(new_t[level]) == ramBitCast<RamSigned>(old_t[level]) &&
                        ramBitCast<RamSigned>(new_t[rule]) < ramBitCast<RamSigned>(old_t[rule]))) {
            old_t[level] = new_t[level];
            old_t[rule] = new_t[rule];
            return true;
        }
        return false;
    }
};

// Alias for btree_set
template <std::size_t Arity, std::size_t AuxiliaryArity>
using Btree = btree_set<t_tuple<Arity>, comparator<Arity>, std::allocator<t_tuple<Arity>>, 256,
        typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - AuxiliaryArity>,
        Updater<Arity, AuxiliaryArity>>;

// Alias for btree_delete_set
template <std::size_t Arity, std::size_t AuxiliaryArity>
using BtreeDelete = btree_delete_set<t_tuple<Arity>, comparator<Arity>, std::allocator<t_tuple<Arity>>, 256,
        typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - AuxiliaryArity>,
        Updater<Arity, AuxiliaryArity>>;

// Alias for Trie
template <std::size_t Arity, std::size_t AuxiliaryArity>
using Brie = Trie<Arity>;

template <std::size_t Arity, std::size_t AuxiliaryArity>
using Provenance = btree_set<t_tuple<Arity>, comparator<Arity>, std::allocator<t_tuple<Arity>>, 256,
        typename detail::default_strategy<t_tuple<Arity>>::type, comparator<Arity - AuxiliaryArity>,
        ProvenanceUpdater<Arity, AuxiliaryArity>>;

// Alias for Eqrel
// Note: require Arity = 2.
template <std::size_t Arity, std::size_t AuxiliaryArity>
using Eqrel = EquivalenceRelation<t_tuple<Arity>>;

};  // namespace souffle::interpreter
