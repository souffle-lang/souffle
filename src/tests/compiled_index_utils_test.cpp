/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file compiled_index_utils.cpp
 *
 * Test cases for the RAM relation data structure.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "CompiledIndexUtils.h"
#include <iosfwd>
#include <string>

namespace souffle {
namespace ram {

using namespace std;

TEST(IndicesTools, AllIndex) {
    using i = index<>;
    using i0 = index<0>;
    using i1 = index<1>;
    using i01 = index<0, 1>;

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
    using i = index<>;
    using i0 = index<0>;
    using i1 = index<1>;
    using i01 = index<0, 1>;

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

}  // namespace ram
}  // end namespace souffle
