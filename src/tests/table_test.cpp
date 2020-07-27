/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file table_test.cpp
 *
 * Test cases for the Table data structure.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "Table.h"
#include <cstddef>
#include <string>

namespace souffle {

namespace test {

template <typename C>
int count(const C& c) {
    int res = 0;
    for (auto it = c.begin(); it != c.end(); ++it) {
        res++;
    }
    return res;
}

TEST(Table, Basic) {
    Table<int> table;
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(0, table.size());
    EXPECT_EQ(0, count(table));

    table.insert(1);

    EXPECT_FALSE(table.empty());
    EXPECT_EQ(1, table.size());
    EXPECT_EQ(1, count(table));
}

TEST(Table, Stress) {
    for (int i = 0; i < 10000; ++i) {
        Table<int> table;

        for (int j = 0; j < i; ++j) {
            table.insert(j);
        }

        EXPECT_EQ((size_t)i, table.size());

        int last = -1;
        for (const auto& cur : table) {
            EXPECT_EQ(last + 1, cur);
            last = cur;
        }
        EXPECT_EQ(last + 1, i);
    }
}
}  // namespace test
}  // end namespace souffle
