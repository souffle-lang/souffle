/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file compiled_ram_index_utils.cpp
 *
 * Test cases for the RAM relation data structure.
 *
 ***********************************************************************/

#include "test.h"

#include "CompiledRamRelation.h"

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

}  // end namespace test
}  // end namespace souffle
