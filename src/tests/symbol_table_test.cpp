/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file symbol_table_test.cpp
 *
 * Tests souffle's symbol table.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/SymbolTable.h"
#include "souffle/datastructure/SymbolTableImpl.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

// How many times to repeat each randomized test
#define RANDOM_TESTS 32
#define RANDOM_TEST_SIZE 32

namespace souffle::test {

static char random_char() {
    static constexpr char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    static constexpr size_t max_index = sizeof(charset) - 1;
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<std::size_t> dist(0, max_index);
    return charset[dist(mt)];
}

static std::string random_string_with_length(std::size_t length) {
    std::string str(length, 0);
    std::generate_n(str.begin(), length, random_char);
    return str;
}

static std::string random_string() {
    // The number 16 was chosen so that the strings are relatively short - both
    // to be light on CPU/memory, and to make collisions possible.
    static constexpr std::size_t max_size = 16;
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<std::size_t> dist(0, max_size);
    return random_string_with_length(dist(mt));
}

TEST(SymbolTable, Basics) {
    SymbolTableImpl table;
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        std::string s = random_string();
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
            // encode/decode
            EXPECT_STREQ(s, table.decode(table.encode(table.decode(table.encode(s)))));
            EXPECT_EQ(table.encode(s), table.encode(table.decode(table.encode(s))));
            EXPECT_STREQ(s, table.decode(table.encode(table.decode(table.encode(s)))));
            EXPECT_EQ(
                    table.encode(s), table.encode(table.decode(table.encode(table.decode(table.encode(s))))));

            // weakContains
            EXPECT_TRUE(table.weakContains(s));

            // findOrInsert
            bool was_new;
            std::tie(std::ignore, was_new) = table.findOrInsert(s);
            EXPECT_TRUE(!was_new);
        }
    }
}

TEST(SymbolTable, Inserts) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        SymbolTableImpl X;
        std::size_t size = rand() % RANDOM_TEST_SIZE;
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int j = 0; j < static_cast<int>(size); ++j) {
            // Guarantee uniqueness by appending something not in the character set
            // and then the index.
            X.encode(random_string() + "~" + std::to_string(j));
        }
        std::vector<std::string> V;
        for (const auto& It : X) {
            EXPECT_TRUE(X.weakContains(It.first));
            V.push_back(It.first);
        }
        EXPECT_EQ(V.size(), size);
    }
}

}  // namespace souffle::test
