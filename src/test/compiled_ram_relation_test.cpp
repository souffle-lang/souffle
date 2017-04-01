/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file compiled_ram_relation_test.cpp
 *
 * Test cases for the RAM relation data structure.
 *
 ***********************************************************************/

#include "CompiledRamRelation.h"
#include "test.h"

namespace souffle {
namespace ram {

using namespace std;

TEST(IndicesTools, AllIndex) {
    typedef index<> i;
    typedef index<0> i0;
    typedef index<1> i1;
    typedef index<0, 1> i01;

    EXPECT_TRUE(index_utils::all_indices<>::value);
    EXPECT_TRUE(index_utils::all_indices<i>::value);
    EXPECT_TRUE((index_utils::all_indices<i1>::value));
    EXPECT_TRUE((index_utils::all_indices<i1>::value));

    EXPECT_TRUE((index_utils::all_indices<i0, i1>::value));
    EXPECT_TRUE((index_utils::all_indices<i0, i1, i01>::value));

    EXPECT_FALSE((index_utils::all_indices<int>::value));
    EXPECT_FALSE((index_utils::all_indices<i1, int, i01>::value));
}

TEST(IndicesTools, Contains) {
    EXPECT_FALSE((index_utils::contains<int>::value));
    EXPECT_FALSE((index_utils::contains<int, double>::value));
    EXPECT_FALSE((index_utils::contains<int, double, float>::value));
    EXPECT_FALSE((index_utils::contains<int, double, float, unsigned>::value));

    EXPECT_TRUE((index_utils::contains<int, int>::value));
    EXPECT_TRUE((index_utils::contains<int, int, int>::value));

    EXPECT_TRUE((index_utils::contains<int, double, int>::value));
    EXPECT_TRUE((index_utils::contains<int, int, double>::value));

    EXPECT_TRUE((index_utils::contains<int, int, double, float>::value));
    EXPECT_TRUE((index_utils::contains<int, double, int, float>::value));
    EXPECT_TRUE((index_utils::contains<int, double, float, int>::value));
}

TEST(IndicesTools, Arity) {
    EXPECT_TRUE((index_utils::check_arity<2, index<0>, index<1>>::value));
    EXPECT_TRUE((index_utils::check_arity<2, index<0, 1>, index<1, 0>>::value));

    EXPECT_FALSE((index_utils::contains<int, double>::value));
    EXPECT_FALSE((index_utils::contains<int, double, float>::value));
    EXPECT_FALSE((index_utils::contains<int, double, float, unsigned>::value));

    EXPECT_TRUE((index_utils::contains<int, int>::value));
    EXPECT_TRUE((index_utils::contains<int, int, int>::value));

    EXPECT_TRUE((index_utils::contains<int, double, int>::value));
    EXPECT_TRUE((index_utils::contains<int, int, double>::value));

    EXPECT_TRUE((index_utils::contains<int, int, double, float>::value));
    EXPECT_TRUE((index_utils::contains<int, double, int, float>::value));
    EXPECT_TRUE((index_utils::contains<int, double, float, int>::value));
}

TEST(IndicesTools, Unique) {
    typedef index<> i;
    typedef index<0> i0;
    typedef index<1> i1;
    typedef index<0, 1> i01;

    EXPECT_TRUE((index_utils::unique<>::value));
    EXPECT_TRUE((index_utils::unique<i>::value));
    EXPECT_TRUE((index_utils::unique<i0>::value));
    EXPECT_TRUE((index_utils::unique<i1, i0>::value));
    EXPECT_TRUE((index_utils::unique<i01, i1, i0>::value));

    EXPECT_FALSE((index_utils::unique<i, i>::value));
    EXPECT_FALSE((index_utils::unique<i, i0, i>::value));
    EXPECT_FALSE((index_utils::unique<i0, int>::value));
}

TEST(IndicesTools, FullIndex) {
    EXPECT_EQ(typeid(index<>), typeid(index_utils::get_full_index<0>::type));
    EXPECT_EQ(typeid(index<0>), typeid(index_utils::get_full_index<1>::type));
    EXPECT_EQ(typeid(index<0, 1>), typeid(index_utils::get_full_index<2>::type));
    EXPECT_EQ(typeid(index<0, 1, 2>), typeid(index_utils::get_full_index<3>::type));
    EXPECT_EQ(typeid(index<0, 1, 2, 3>), typeid(index_utils::get_full_index<4>::type));
}

TEST(IndicesTools, ExtendToFullIndex) {
    EXPECT_EQ(typeid(index<>), typeid(index_utils::extend_to_full_index<0, index<>>::type));
    EXPECT_EQ(typeid(index<0>), typeid(index_utils::extend_to_full_index<1, index<>>::type));
    EXPECT_EQ(typeid(index<0, 1>), typeid(index_utils::extend_to_full_index<2, index<>>::type));
    EXPECT_EQ(typeid(index<0, 1, 2>), typeid(index_utils::extend_to_full_index<3, index<>>::type));

    EXPECT_EQ(typeid(index<0>), typeid(index_utils::extend_to_full_index<1, index<0>>::type));
    EXPECT_EQ(typeid(index<1, 0>), typeid(index_utils::extend_to_full_index<2, index<1>>::type));
    EXPECT_EQ(typeid(index<2, 0, 1>), typeid(index_utils::extend_to_full_index<3, index<2>>::type));

    EXPECT_EQ(typeid(index<1, 0>), typeid(index_utils::extend_to_full_index<2, index<1, 0>>::type));
    EXPECT_EQ(typeid(index<1, 0, 2>), typeid(index_utils::extend_to_full_index<3, index<1, 0>>::type));
    EXPECT_EQ(typeid(index<1, 0, 2, 3>), typeid(index_utils::extend_to_full_index<4, index<1, 0>>::type));
}

TEST(IndicesTools, IsPrefix) {
    // those should work:
    EXPECT_TRUE((index_utils::is_prefix<index<>, index<>>::value));
    EXPECT_TRUE((index_utils::is_prefix<index<>, index<0>>::value));
    EXPECT_TRUE((index_utils::is_prefix<index<>, index<1>>::value));

    EXPECT_TRUE((index_utils::is_prefix<index<0>, index<0>>::value));
    EXPECT_TRUE((index_utils::is_prefix<index<0>, index<0, 0>>::value));
    EXPECT_TRUE((index_utils::is_prefix<index<1>, index<1, 0>>::value));

    EXPECT_TRUE((index_utils::is_prefix<index<1, 0>, index<1, 0, 1>>::value));
    EXPECT_TRUE((index_utils::is_prefix<index<1, 0, 1>, index<1, 0, 1>>::value));

    // a few false checks
    EXPECT_FALSE((index_utils::is_prefix<index<0>, index<>>::value));
    EXPECT_FALSE((index_utils::is_prefix<index<0, 1>, index<1>>::value));
    EXPECT_FALSE((index_utils::is_prefix<index<1, 0>, index<0, 1>>::value));
}

TEST(IndicesTools, IsPermutation) {
    // those should work:
    EXPECT_TRUE((index_utils::is_permutation<index<0, 1>, index<0, 1>>::value));
    EXPECT_TRUE((index_utils::is_permutation<index<0, 1>, index<1, 0>>::value));
    EXPECT_TRUE((index_utils::is_permutation<index<2, 1, 4, 0, 3>, index<4, 1, 0, 3, 2>>::value));

    // a few false checks
    EXPECT_FALSE((index_utils::is_permutation<index<0>, index<1>>::value));
    EXPECT_FALSE((index_utils::is_permutation<index<0, 1>, index<1>>::value));
    EXPECT_FALSE((index_utils::is_permutation<index<0, 1>, index<0, 2>>::value));
}

TEST(IndicesTools, IsCompatibleWith) {
    // those should work:
    EXPECT_TRUE((index_utils::is_compatible_with<index<0>, index<0, 1>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<0, 1>, index<0, 1>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<1, 0>, index<0, 1>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<0, 1>, index<0, 1, 2>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<1, 0>, index<0, 1, 2>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<0, 1, 2>, index<0, 1, 2>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<0, 2, 1>, index<0, 1, 2>>::value));
    EXPECT_TRUE((index_utils::is_compatible_with<index<2, 1, 0>, index<0, 1, 2>>::value));

    // a few false checks
    EXPECT_FALSE((index_utils::is_compatible_with<index<0>, index<1>>::value));
    EXPECT_FALSE((index_utils::is_compatible_with<index<1>, index<0, 1>>::value));
    EXPECT_FALSE((index_utils::is_compatible_with<index<0, 1>, index<1>>::value));
    EXPECT_FALSE((index_utils::is_compatible_with<index<0, 1>, index<0, 2>>::value));
}

TEST(IndicesTools, GetPrefix) {
    EXPECT_EQ(typeid(index<>), typeid(index_utils::get_prefix<0, index<0, 1, 3, 2>>::type));
    EXPECT_EQ(typeid(index<0>), typeid(index_utils::get_prefix<1, index<0, 1, 3, 2>>::type));
    EXPECT_EQ(typeid(index<0, 1>), typeid(index_utils::get_prefix<2, index<0, 1, 3, 2>>::type));
    EXPECT_EQ(typeid(index<0, 1, 3>), typeid(index_utils::get_prefix<3, index<0, 1, 3, 2>>::type));
    EXPECT_EQ(typeid(index<0, 1, 3, 2>), typeid(index_utils::get_prefix<4, index<0, 1, 3, 2>>::type));
}

TEST(Relation, Basic) {
    typedef Relation<Auto, 2> relation_t;

    relation_t data;

    EXPECT_TRUE(data.empty());
    EXPECT_EQ(0, data.size());

    EXPECT_FALSE(data.contains(1, 2));
    EXPECT_FALSE(data.contains(2, 2));

    data.insert(1, 2);

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(1, data.size());
    EXPECT_TRUE(data.contains(1, 2));
    EXPECT_FALSE(data.contains(2, 2));

    data.insert(1, 2);

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(1, data.size());
    EXPECT_TRUE(data.contains(1, 2));
    EXPECT_FALSE(data.contains(2, 2));

    data.insert(2, 2);

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(2, data.size());
    EXPECT_TRUE(data.contains(1, 2));
    EXPECT_TRUE(data.contains(2, 2));

    for (const auto& cur : data) {
        std::cout << cur << "\n";
    }
}

TEST(Relation, Structure_Auto) {
    // check the proper instantiation of a few relations
    EXPECT_EQ("Nullary Relation", (Relation<Auto, 0>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=1 based on a trie-index(<0>)",
            (Relation<Auto, 1>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=2 based on a trie-index(<0,1>)",
            (Relation<Auto, 2>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a direct-btree-index(<0,1,2>)",
            (Relation<Auto, 3>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=4 based on a direct-btree-index(<0,1,2,3>)",
            (Relation<Auto, 4>().getDescription()));

    EXPECT_EQ("Index-Organized Relation of arity=1 based on a trie-index(<0>)",
            (Relation<Auto, 1, index<0>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=2 based on a trie-index(<1,0>)",
            (Relation<Auto, 2, index<1>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a direct-btree-index(<2,0,1>)",
            (Relation<Auto, 3, index<2>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a direct-btree-index(<1,0,2>)",
            (Relation<Auto, 3, index<1>>()).getDescription());

    // most of it should be direct indices
    EXPECT_EQ(
            "DirectIndexedRelation of arity=2 with indices [ trie-index(<0,1>) trie-index(<1,0>)  ] where "
            "<0,1> is the primary index",
            (Relation<Auto, 2, index<0, 1>, index<1, 0>>()).getDescription());

    // partial indices are becoming full indices for small arities
    EXPECT_EQ(
            "DirectIndexedRelation of arity=2 with indices [ trie-index(<0,1>) trie-index(<1,0>)  ] where "
            "<0,1> is the primary index",
            (Relation<Auto, 2, index<0, 1>, index<1>>()).getDescription());

    // partial indices are becoming full indices for small arities
    EXPECT_EQ(
            "DirectIndexedRelation of arity=3 with indices [ direct-btree-index(<0,2,1>) "
            "direct-btree-index(<1,0,2>)  ] where <0,2,1> is the primary index",
            (Relation<Auto, 3, index<0, 2, 1>, index<1>>()).getDescription());

    // TODO: filter indices that are prefixes of others

    // test larger relations
    EXPECT_EQ(
            "Relation of arity=8 with indices [ indirect-btree-index(<0,1,2,3,4,5,6,7>) "
            "indirect-btree-index(<0,1,2>) indirect-btree-index(<2,3,4>)  ] where <0,1,2,3,4,5,6,7> is the "
            "primary index",
            (Relation<Auto, 8, index<0, 1, 2>, index<2, 3, 4>>()).getDescription());
}

TEST(Relation, Structure_BTree) {
    // check the proper instantiation of a few relations
    EXPECT_EQ("Nullary Relation", (Relation<BTree, 0>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=1 based on a direct-btree-index(<0>)",
            (Relation<BTree, 1>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=2 based on a direct-btree-index(<0,1>)",
            (Relation<BTree, 2>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a direct-btree-index(<0,1,2>)",
            (Relation<BTree, 3>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=4 based on a direct-btree-index(<0,1,2,3>)",
            (Relation<BTree, 4>().getDescription()));

    EXPECT_EQ("Index-Organized Relation of arity=1 based on a direct-btree-index(<0>)",
            (Relation<BTree, 1, index<0>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=2 based on a direct-btree-index(<1,0>)",
            (Relation<BTree, 2, index<1>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a direct-btree-index(<2,0,1>)",
            (Relation<BTree, 3, index<2>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a direct-btree-index(<1,0,2>)",
            (Relation<BTree, 3, index<1>>()).getDescription());

    // most of it should be direct indices
    EXPECT_EQ(
            "DirectIndexedRelation of arity=2 with indices [ direct-btree-index(<0,1>) "
            "direct-btree-index(<1,0>)  ] where <0,1> is the primary index",
            (Relation<BTree, 2, index<0, 1>, index<1, 0>>()).getDescription());

    // partial indices are becoming full indices for small arities
    EXPECT_EQ(
            "DirectIndexedRelation of arity=2 with indices [ direct-btree-index(<0,1>) "
            "direct-btree-index(<1,0>)  ] where <0,1> is the primary index",
            (Relation<BTree, 2, index<0, 1>, index<1>>()).getDescription());

    // partial indices are becoming full indices for small arities
    EXPECT_EQ(
            "DirectIndexedRelation of arity=3 with indices [ direct-btree-index(<0,2,1>) "
            "direct-btree-index(<1,0,2>)  ] where <0,2,1> is the primary index",
            (Relation<BTree, 3, index<0, 2, 1>, index<1>>()).getDescription());

    // TODO: filter indices that are prefixes of others

    // test larger relations
    EXPECT_EQ(
            "DirectIndexedRelation of arity=8 with indices [ direct-btree-index(<0,1,2,3,4,5,6,7>) "
            "direct-btree-index(<2,3,4,0,1,5,6,7>)  ] where <0,1,2,3,4,5,6,7> is the primary index",
            (Relation<BTree, 8, index<0, 1, 2>, index<2, 3, 4>>()).getDescription());
}

TEST(Relation, Structure_Brie) {
    // check the proper instantiation of a few relations
    EXPECT_EQ("Nullary Relation", (Relation<Brie, 0>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=1 based on a trie-index(<0>)",
            (Relation<Brie, 1>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=2 based on a trie-index(<0,1>)",
            (Relation<Brie, 2>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a trie-index(<0,1,2>)",
            (Relation<Brie, 3>().getDescription()));
    EXPECT_EQ("Index-Organized Relation of arity=4 based on a trie-index(<0,1,2,3>)",
            (Relation<Brie, 4>().getDescription()));

    EXPECT_EQ("Index-Organized Relation of arity=1 based on a trie-index(<0>)",
            (Relation<Brie, 1, index<0>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=2 based on a trie-index(<1,0>)",
            (Relation<Brie, 2, index<1>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a trie-index(<2,0,1>)",
            (Relation<Brie, 3, index<2>>()).getDescription());
    EXPECT_EQ("Index-Organized Relation of arity=3 based on a trie-index(<1,0,2>)",
            (Relation<Brie, 3, index<1>>()).getDescription());

    // most of it should be direct indices
    EXPECT_EQ(
            "DirectIndexedRelation of arity=2 with indices [ trie-index(<0,1>) trie-index(<1,0>)  ] where "
            "<0,1> is the primary index",
            (Relation<Brie, 2, index<0, 1>, index<1, 0>>()).getDescription());

    // partial indices are becoming full indices for small arities
    EXPECT_EQ(
            "DirectIndexedRelation of arity=2 with indices [ trie-index(<0,1>) trie-index(<1,0>)  ] where "
            "<0,1> is the primary index",
            (Relation<Brie, 2, index<0, 1>, index<1>>()).getDescription());

    // partial indices are becoming full indices for small arities
    EXPECT_EQ(
            "DirectIndexedRelation of arity=3 with indices [ trie-index(<0,2,1>) trie-index(<1,0,2>)  ] "
            "where <0,2,1> is the primary index",
            (Relation<Brie, 3, index<0, 2, 1>, index<1>>()).getDescription());

    // TODO: filter indices that are prefixes of others

    // test larger relations
    EXPECT_EQ(
            "DirectIndexedRelation of arity=8 with indices [ trie-index(<0,1,2,3,4,5,6,7>) "
            "trie-index(<2,3,4,0,1,5,6,7>)  ] where <0,1,2,3,4,5,6,7> is the primary index",
            (Relation<Brie, 8, index<0, 1, 2>, index<2, 3, 4>>()).getDescription());
}

TEST(Relation, BigTuple) {
    typedef Relation<Auto, 5> relation_t;

    relation_t data;

    EXPECT_TRUE(data.empty());
    EXPECT_EQ(0, data.size());

    EXPECT_FALSE(data.contains(1, 2, 3, 4, 5));
    EXPECT_FALSE(data.contains(2, 2, 3, 3, 5));

    data.insert(1, 2, 3, 4, 5);

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(1, data.size());
    EXPECT_TRUE(data.contains(1, 2, 3, 4, 5));
    EXPECT_FALSE(data.contains(2, 2, 3, 3, 5));

    data.insert(1, 2, 3, 4, 5);

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(1, data.size());
    EXPECT_TRUE(data.contains(1, 2, 3, 4, 5));
    EXPECT_FALSE(data.contains(2, 2, 3, 3, 5));

    data.insert(2, 2, 3, 3, 5);

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(2, data.size());
    EXPECT_TRUE(data.contains(1, 2, 3, 4, 5));
    EXPECT_TRUE(data.contains(2, 2, 3, 3, 5));
}

TEST(Relation, Indices) {
    int count = 0;

    Relation<Auto, 2, index<0>, index<1>> data;
    typedef decltype(data)::tuple_type tuple_t;

    EXPECT_EQ(2 * sizeof(RamDomain), sizeof(tuple_t));

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            data.insert(i, j);
        }
    }

    // print full table
    count = 0;
    std::cout << "Table data: \n";
    for (const auto& cur : data) {
        std::cout << cur << "\n";
        count++;
    }
    std::cout << "\n";

    EXPECT_EQ(5 * 5, count);

    tuple_t x = {{3, 0}};

    // according to first index
    count = 0;
    std::cout << "Index (0,1): \n";
    for (const auto& cur : data.equalRange<0>(x)) {
        std::cout << cur << "\n";
        count++;
    }
    std::cout << "\n";

    EXPECT_EQ(5, count);

    // according to the second index
    x = {{0, 3}};

    count = 0;
    std::cout << "Index (1,0): \n";
    for (const auto& cur : data.equalRange<1>(x)) {
        std::cout << cur << "\n";
        count++;
    }
    std::cout << "\n";

    EXPECT_EQ(5, count);

    // ----- equal range ---------

    x = {{2, 3}};

    // according to the first index
    count = 0;
    std::cout << "Index (0,1): \n";
    for (const auto& cur : data.equalRange<0, 1>(x)) {
        std::cout << cur << "\n";
        count++;
    }
    std::cout << "\n";
    EXPECT_EQ(1, count);

    // according to the second index
    count = 0;
    std::cout << "Index (1,0): \n";
    for (const auto& cur : data.equalRange<1, 0>(x)) {
        std::cout << cur << "\n";
        count++;
    }
    std::cout << "\n";
    EXPECT_EQ(1, count);

    // according to the third index
    count = 0;
    std::cout << "Index (0): \n";
    for (const auto& cur : data.equalRange<0>(x)) {
        std::cout << cur << "\n";
        EXPECT_EQ(x[0], cur[0]);
        count++;
    }
    std::cout << "\n";
    EXPECT_EQ(5, count);

    // according to the fourth index
    count = 0;
    std::cout << "Index (1): \n";
    for (const auto& cur : data.equalRange<1>(x)) {
        std::cout << cur << "\n";
        EXPECT_EQ(x[1], cur[1]);
        count++;
    }
    std::cout << "\n";
    EXPECT_EQ(5, count);
}

TEST(Relation, EqualRange) {
    Relation<Auto, 2, index<0, 1>, index<1, 0>> rel;
    typedef decltype(rel)::tuple_type tuple_t;

    for (int i = 0; i < 5; i++) {
        for (int j = 3; j < 8; j++) {
            rel.insert(i, j);
        }
    }

    std::set<tuple_t> set;
    tuple_t pattern;

    //        // test index 0
    //        set.clear();
    //        pattern = {2,4};
    //        for(const auto& cur : rel.equalRange<0>(pattern)) {
    //            set.insert(cur);
    //        }
    //        EXPECT_EQ("{[2,4]}", toString(set));
    //
    //
    //        // test index 1
    //        set.clear();
    //        pattern = {2,4};
    //        for(const auto& cur : rel.equalRange<1>(pattern)) {
    //            set.insert(cur);
    //        }
    //        EXPECT_EQ("{[2,4]}", toString(set));

    // test index 0,1
    set.clear();
    pattern = {{2, 4}};
    for (const auto& cur : rel.equalRange<0, 1>(pattern)) {
        set.insert(cur);
    }
    EXPECT_EQ("{[2,4]}", toString(set));

    // test index 1,0
    set.clear();
    pattern = {{2, 4}};
    for (const auto& cur : rel.equalRange<1, 0>(pattern)) {
        set.insert(cur);
    }
    EXPECT_EQ("{[2,4]}", toString(set));
}

TEST(Relation, NullArity) {
    Relation<Auto, 0> rel;
    EXPECT_EQ(0, sizeof(Relation<Auto, 0>::tuple_type));  // strange, but true

    EXPECT_EQ(0, rel.size());
    EXPECT_TRUE(rel.empty());
    EXPECT_FALSE(rel.contains());

    rel.insert();
    EXPECT_EQ(1, rel.size());
    EXPECT_FALSE(rel.empty());
    EXPECT_TRUE(rel.contains());

    rel.insert();
    EXPECT_EQ(1, rel.size());
    EXPECT_FALSE(rel.empty());
    EXPECT_TRUE(rel.contains());

    rel.purge();
    EXPECT_EQ(0, rel.size());
    EXPECT_TRUE(rel.empty());
    EXPECT_FALSE(rel.contains());

    EXPECT_EQ(rel.begin(), rel.end());
    rel.insert();
    EXPECT_FALSE(rel.empty());
    EXPECT_NE(rel.begin(), rel.end());

    int count = 0;
    for (const auto& cur : rel) {
        std::cout << cur << "\n";
        count++;
    }
    EXPECT_EQ(1, count);
}

template <typename C>
int count(const C& c) {
    int res = 0;
    for (auto it = c.begin(); it != c.end(); ++it) res++;
    return res;
}

TEST(Relation, SingleIndex) {
    Relation<Auto, 2, index<1, 0>> rel;

    EXPECT_TRUE(rel.empty());
    EXPECT_EQ(0, rel.size());
    EXPECT_EQ(rel.begin(), rel.end());
    EXPECT_FALSE(rel.contains(1, 2));
    EXPECT_FALSE(rel.contains(2, 1));
    EXPECT_EQ(0, count(rel));

    rel.insert(1, 2);

    EXPECT_FALSE(rel.empty());
    EXPECT_EQ(1, rel.size());
    EXPECT_NE(rel.begin(), rel.end());
    EXPECT_TRUE(rel.contains(1, 2));
    EXPECT_FALSE(rel.contains(2, 1));
    EXPECT_EQ(1, count(rel));

    rel.insert(2, 1);

    EXPECT_FALSE(rel.empty());
    EXPECT_EQ(2, rel.size());
    EXPECT_NE(rel.begin(), rel.end());
    EXPECT_TRUE(rel.contains(1, 2));
    EXPECT_TRUE(rel.contains(2, 1));
    EXPECT_EQ(2, count(rel));

    rel.insert(2, 1);

    EXPECT_FALSE(rel.empty());
    EXPECT_EQ(2, rel.size());
    EXPECT_NE(rel.begin(), rel.end());
    EXPECT_TRUE(rel.contains(1, 2));
    EXPECT_TRUE(rel.contains(2, 1));
    EXPECT_EQ(2, count(rel));
}

template <typename T>
void consume(T t) {}

TEST(Relation, SingleIndexEqualRange) {
    typedef Relation<Auto, 3, index<0>> rel_type;
    typedef typename rel_type::tuple_type tuple_type;

    rel_type rel;

    // fill relation
    for (int x = 1; x <= 5; x++)
        for (int y = 1; y <= 5; y++)
            for (int z = 1; z <= 5; z++) rel.insert(x, y, z);

    int count = 0;
    for (const auto& cur : rel) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5 * 5 * 5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<>(tuple_type({{3, 2, 1}}))) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5 * 5 * 5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<0>(tuple_type({{3, 2, 1}}))) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(1 * 5 * 5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<0, 1>(tuple_type({{3, 2, 1}}))) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(1 * 1 * 5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<0, 1, 2>(tuple_type({{3, 2, 1}}))) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(1 * 1 * 1, count);
}

TEST(Relation, SingleIndexLowerUpperBound) {
    typedef Relation<Auto, 3, index<0>> rel_type;
    typedef typename rel_type::tuple_type tuple_type;

    rel_type rel;

    // fill relation
    for (int x = 1; x <= 5; x++)
        for (int y = 1; y <= 5; y++)
            for (int z = 1; z <= 5; z++) rel.insert(x, y, z);

    int count = 0;
    for (const auto& cur : rel) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5 * 5 * 5, count);

    tuple_type x = {{3, 3, 3}};

    count = 0;
    for (const auto& cur : rel.equalRange<>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5 * 5 * 5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<0>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5 * 5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<0, 1>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<1, 0>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(5, count);

    count = 0;
    for (const auto& cur : rel.equalRange<0, 1, 2>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(1, count);

    count = 0;
    for (const auto& cur : rel.equalRange<2, 1, 0>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(1, count);

    count = 0;
    for (const auto& cur : rel.equalRange<1, 0, 2>(x)) {
        consume(cur);
        count++;
    }
    EXPECT_EQ(1, count);
}

TEST(Relation, Partition_0D) {
    typedef Relation<Auto, 0> rel_type;
    typedef typename rel_type::tuple_type tuple_type;

    rel_type rel;

    // fill relation
    rel.insert();

    // create partition
    auto partition = rel.partition();

    // assume that there are more than 1 elements in the partition
    EXPECT_EQ(1, partition.size());

    // and all of them are non-empty
    for (const auto& cur : partition) {
        EXPECT_FALSE(cur.empty());
    }

    // iterate through partitions
    std::set<tuple_type> elements;
    for (const auto& part : partition) {
        for (const auto& cur : part) {
            EXPECT_TRUE(elements.insert(cur).second) << "Duplication of element: " << cur;
        }
    }

    EXPECT_EQ(1, elements.size());
}

TEST(Relation, Partition_1D) {
    const int N = 1000;

    typedef Relation<Auto, 1, index<0>> rel_type;
    typedef typename rel_type::tuple_type tuple_type;

    rel_type rel;

    // fill relation
    for (int i = 0; i < N; i++) {
        rel.insert(i);
    }

    // create partition
    auto partition = rel.partition();

    // assume that there are more than 1 elements in the partition
    EXPECT_LT(1, partition.size());

    // and all of them are non-empty
    for (const auto& cur : partition) {
        EXPECT_FALSE(cur.empty());
    }

    // iterate through partitions
    std::set<tuple_type> elements;
    for (const auto& part : partition) {
        for (const auto& cur : part) {
            EXPECT_TRUE(elements.insert(cur).second) << "Duplication of element: " << cur;
        }
    }

    EXPECT_EQ(N, elements.size());
}

TEST(Relation, Partition_2D) {
    const int N = 1000;

    typedef Relation<Auto, 2, index<0, 1>> rel_type;
    typedef typename rel_type::tuple_type tuple_type;

    rel_type rel;

    // fill relation
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            rel.insert(i, j);
        }
    }

    // create partition
    auto partition = rel.partition();

    // assume that there are more than 1 elements in the partition
    EXPECT_LT(1, partition.size());

    // and all of them are non-empty
    for (const auto& cur : partition) {
        EXPECT_FALSE(cur.empty());
    }

    // iterate through partitions
    std::set<tuple_type> elements;
    for (const auto& part : partition) {
        for (const auto& cur : part) {
            EXPECT_TRUE(elements.insert(cur).second) << "Duplication of element: " << cur;
        }
    }

    EXPECT_EQ(N * N, elements.size());
}

TEST(Relation, PartitionBug_InsertAll) {
    typedef Relation<Auto, 2> rel_type;
    typedef typename rel_type::tuple_type tuple_type;

    // a bug encountered during development:
    Relation<Auto, 2, ram::index<0, 1>> relA;
    relA.insert(2, 0);
    relA.insert(0, 3);
    relA.insert(3, 4);
    relA.insert(4, 6);
    relA.insert(6, 7);
    relA.insert(8, 9);
    relA.insert(9, 11);
    relA.insert(11, 12);

    Relation<Auto, 2> rel;
    rel.insertAll(relA);

    std::set<tuple_type> all;
    for (const auto& cur : relA) {
        all.insert(cur);
    }

    std::set<tuple_type> is;
    auto part = rel.partition();
    for (const auto& p : part) {
        for (const auto& c : p) {
            EXPECT_TRUE(is.insert(c).second) << "Duplicate: " << c;
        }
    }

    EXPECT_EQ(all, is);
}

}  // end namespace test
}  // end namespace souffle
