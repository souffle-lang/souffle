/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file compiled_tuple_test.cpp
 *
 * Test cases for the RAM tuple data structure.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "CompiledTuple.h"
#include <cstdint>
#include <iostream>
#include <string>

namespace souffle {

namespace ram {

TEST(Tuple, Basic) {
    Tuple<int, 3> t = {{1, 3, 2}};

    EXPECT_EQ(3 * sizeof(int), sizeof(t));

    std::cout << t << "\n";

    Tuple<int, 2> t2 = {{1, 5}};
    EXPECT_EQ(2 * sizeof(int), sizeof(t2));
    std::cout << t2 << "\n";
}

TEST(Tuple, Assign) {
    Tuple<int, 3> t1 = {{1, 2, 3}};
    Tuple<int, 3> t2 = {{3, 2, 1}};

    Tuple<int, 3> t3 = t1;

    EXPECT_NE(t1, t2);
    EXPECT_EQ(t1, t3);
    EXPECT_NE(t2, t3);

    t3 = t2;

    EXPECT_NE(t1, t2);
    EXPECT_NE(t1, t3);
    EXPECT_EQ(t2, t3);
}

TEST(Tuple, Compare) {
    Tuple<int, 2> t1 = {{1, 2}};
    Tuple<int, 2> t2 = {{2, 1}};

    EXPECT_LT(t1, t2);
}

TEST(Tuple, CompareSpeed) {
    // was used to evaluate various implementations of the == operator

    Tuple<int, 2> t1 = {{1, 2}};
    Tuple<int, 2> t2 = {{2, 1}};

    uint32_t res = 0;
    while (true) {
        res += !(t1 == t2);
        if (res & 0x10000000llu) break;
    }
    EXPECT_EQ(268435456llu, res);
}

}  // namespace ram
}  // end namespace souffle
