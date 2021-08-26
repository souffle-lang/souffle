/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file util_test.cpp
 *
 * Test cases for utilities.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/utility/CacheUtil.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

using namespace souffle;

TEST(Util, toString) {
    EXPECT_EQ("12", toString(12));
    EXPECT_EQ("Hello", toString("Hello"));
}

TEST(Util, toVector) {
    EXPECT_EQ("[1,2,3]", toString(toVector(1, 2, 3)));
    EXPECT_EQ("[]", toString(toVector<int>()));
}

TEST(Util, printVector) {
    std::vector<int> v;

    EXPECT_EQ("[]", toString(v));
    v.push_back(12);
    EXPECT_EQ("[12]", toString(v));
    v.push_back(14);
    EXPECT_EQ("[12,14]", toString(v));
}

TEST(Util, printSet) {
    std::set<int> v;

    EXPECT_EQ("{}", toString(v));
    v.insert(12);
    EXPECT_EQ("{12}", toString(v));
    v.insert(14);
    EXPECT_EQ("{12,14}", toString(v));
}

TEST(Util, printMap) {
    std::map<int, std::string> m;

    EXPECT_EQ("{}", toString(m));
    m[12] = "Hello";
    EXPECT_EQ("{12->Hello}", toString(m));
    m[14] = "World";
    EXPECT_EQ("{12->Hello,14->World}", toString(m));
}

TEST(Util, LambdaTraits) {
    auto lambda = [](int) -> bool { return true; };

    EXPECT_EQ(typeid(bool).name(), typeid(lambda_traits<decltype(lambda)>::result_type).name());
    EXPECT_EQ(typeid(int).name(), typeid(lambda_traits<decltype(lambda)>::template arg<0>).name());
}

TEST(Util, LRUCache) {
    using cache = LRUCache<int, 4>;
    cache c;

    // check initial state
    EXPECT_EQ("0,0,0,0", toString(c));

    // fill cache with new entries
    c.access(4);
    EXPECT_EQ("4,0,0,0", toString(c));

    c.access(3);
    EXPECT_EQ("3,4,0,0", toString(c));

    c.access(7);
    EXPECT_EQ("7,3,4,0", toString(c));

    c.access(2);
    EXPECT_EQ("2,7,3,4", toString(c));

    c.access(5);
    EXPECT_EQ("5,2,7,3", toString(c));

    // access elements present
    c.access(5);
    EXPECT_EQ("5,2,7,3", toString(c));

    c.access(2);
    EXPECT_EQ("2,5,7,3", toString(c));

    c.access(7);
    EXPECT_EQ("7,2,5,3", toString(c));

    c.access(3);
    EXPECT_EQ("3,7,2,5", toString(c));

    c.access(2);
    EXPECT_EQ("2,3,7,5", toString(c));

    // test clearing
    c.clear();
    EXPECT_EQ("0,0,0,0", toString(c));

    c.clear(5);
    EXPECT_EQ("5,5,5,5", toString(c));
}

TEST(Util, LRUCache_S2) {
    using cache = LRUCache<int, 2>;
    cache c;

    // check initial state
    EXPECT_EQ("0,0", toString(c));

    // fill cache with new entries
    c.access(4);
    EXPECT_EQ("4,0", toString(c));

    c.access(3);
    EXPECT_EQ("3,4", toString(c));

    c.access(7);
    EXPECT_EQ("7,3", toString(c));

    // access elements present
    c.access(7);
    EXPECT_EQ("7,3", toString(c));

    c.access(3);
    EXPECT_EQ("3,7", toString(c));

    c.access(7);
    EXPECT_EQ("7,3", toString(c));

    // test clearing
    c.clear();
    EXPECT_EQ("0,0", toString(c));

    c.clear(5);
    EXPECT_EQ("5,5", toString(c));
}

TEST(Util, LRUCache_S1) {
    using cache = LRUCache<int, 1>;
    cache c;

    // check initial state
    EXPECT_EQ("0", toString(c));

    // fill cache with new entries
    c.access(4);
    EXPECT_EQ("4", toString(c));

    c.access(3);
    EXPECT_EQ("3", toString(c));

    // access elements present
    c.access(3);
    EXPECT_EQ("3", toString(c));

    // and again not present
    c.access(2);
    EXPECT_EQ("2", toString(c));

    // test clearing
    c.clear();
    EXPECT_EQ("0", toString(c));

    c.clear(5);
    EXPECT_EQ("5", toString(c));
}

TEST(Util, LRUCache_S0) {
    using cache = LRUCache<int, 0>;
    cache c;

    // check initial state
    EXPECT_EQ("-empty-", toString(c));

    // fill cache with new entries
    c.access(4);
    EXPECT_EQ("-empty-", toString(c));

    c.access(3);
    EXPECT_EQ("-empty-", toString(c));

    // access elements present
    c.access(3);
    EXPECT_EQ("-empty-", toString(c));

    // and again not present
    c.access(2);
    EXPECT_EQ("-empty-", toString(c));

    // test clearing
    c.clear();
    EXPECT_EQ("-empty-", toString(c));

    c.clear(5);
    EXPECT_EQ("-empty-", toString(c));
}

TEST(Util, Range) {
    using container = std::vector<int>;
    using iter = container::const_iterator;

    std::vector<int> list(9);

    for (int i = 0; i < 9; i++) {
        list[i] = i;
    }

    range<iter> range(list.begin(), list.end());

    {
        int last = -1;
        for (const auto& p : range.partition(4)) {
            auto size = p.end() - p.begin();
            EXPECT_TRUE(2 <= size && size <= 3);
            for (const auto& cur : p) {
                EXPECT_EQ(last + 1, cur);
                last = cur;
            }
        }
        EXPECT_EQ(last, 8);
    }

    {
        int last = -1;
        for (const auto& p : range.partition(7)) {
            auto size = p.end() - p.begin();
            EXPECT_TRUE(1 <= size && size <= 2);
            for (const auto& cur : p) {
                EXPECT_EQ(last + 1, cur);
                last = cur;
            }
        }
        EXPECT_EQ(last, 8);
    }
}
