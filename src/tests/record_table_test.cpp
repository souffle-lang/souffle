/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file record_table_test.cpp
 *
 * Tests the record table.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
#include "souffle/datastructure/RecordTableImpl.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <cstddef>

namespace souffle::test {

#define NUMBER_OF_TESTS 100

TEST(Pack, Tuple) {
    SpecializedRecordTable<3> recordTable;
    const Tuple<RamDomain, 3> tuple = {{1, 2, 3}};

    RamDomain ref = pack(recordTable, tuple);

    const RamDomain* ptr = recordTable.unpack(ref, 3);

    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(tuple[i], ptr[i]);
    }
}

TEST(Pack, InitList) {
    SpecializedRecordTable<3> recordTable;

    RamDomain ref = recordTable.pack({1, 2, 3});

    const RamDomain* ptr = recordTable.unpack(ref, 3);

    EXPECT_EQ(1, ptr[0]);
    EXPECT_EQ(2, ptr[1]);
    EXPECT_EQ(3, ptr[2]);
}

TEST(Pack, InitListHelper) {
    SpecializedRecordTable<3> recordTable;

    RamDomain ref = pack(recordTable, {1, 2, 3});

    const RamDomain* ptr = recordTable.unpack(ref, 3);

    EXPECT_EQ(1, ptr[0]);
    EXPECT_EQ(2, ptr[1]);
    EXPECT_EQ(3, ptr[2]);
}

// Generate random tuples
// pack them all
// unpack and test for equality
TEMPLATE_TEST(PackUnpack, Tuple, std::size_t TupleSize, TupleSize) {
    using tupleType = Tuple<RamDomain, TupleSize>;
    constexpr std::size_t tupleSize = TupleSize;

    SpecializedRecordTable<3> recordTable;

    // Setup random number generation
    std::default_random_engine randomGenerator(3);
    std::uniform_int_distribution<RamDomain> distribution(
            std::numeric_limits<RamDomain>::lowest(), std::numeric_limits<RamDomain>::max());

    auto random = std::bind(distribution, randomGenerator);
    auto rnd = [&]() { return random(); };

    // Tuples that will be packed
    std::vector<tupleType> toPack(NUMBER_OF_TESTS);

    // Tuple reference after they are packed.
    std::vector<RamDomain> tupleRef(NUMBER_OF_TESTS);

    // Generate and pack the tuples
    for (std::size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        std::generate(toPack[i].begin(), toPack[i].end(), rnd);
        tupleRef[i] = pack(recordTable, toPack[i]);
        EXPECT_LT(0, tupleRef[i]);
    }

    // unpack and test
    for (std::size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        const RamDomain* unpacked = recordTable.unpack(tupleRef[i], tupleSize);
        tupleType cmp;
        std::copy_n(unpacked, TupleSize, cmp.begin());
        EXPECT_EQ(toPack[i], cmp);
    }
}

INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 0);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 1);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 2);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 3);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 4);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 5);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 6);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 7);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 11);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 23);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Tuple, 59);

// Generate random vectors
// pack them all
// unpack and test for equality
TEMPLATE_TEST(PackUnpack, Vector, std::size_t VectorSize, VectorSize) {
    constexpr std::size_t vectorSize = VectorSize;

    // Setup random number generation
    std::default_random_engine randomGenerator(3);
    std::uniform_int_distribution<RamDomain> distribution(
            std::numeric_limits<RamDomain>::lowest(), std::numeric_limits<RamDomain>::max());

    auto random = std::bind(distribution, randomGenerator);
    auto rnd = [&]() { return random(); };

    SpecializedRecordTable<VectorSize> recordTable;

    // Tuples that will be packed
    std::vector<std::vector<RamDomain>> toPack(NUMBER_OF_TESTS);

    // Tuple reference after they are packed.
    std::vector<RamDomain> tupleRef(NUMBER_OF_TESTS);

    // Generate and pack the tuples
    for (std::size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        toPack[i].resize(vectorSize);
        std::generate(toPack[i].begin(), toPack[i].end(), rnd);
        tupleRef[i] = recordTable.pack(toPack[i].data(), vectorSize);
        EXPECT_LT(0, tupleRef[i]);
    }

    // unpack and test
    for (std::size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        const RamDomain* unpacked{recordTable.unpack(tupleRef[i], vectorSize)};
        for (std::size_t j = 0; j < vectorSize; ++j) {
            EXPECT_EQ(toPack[i][j], unpacked[j]);
        }
    }
}

// special version of the test for vector of size 0
SPECIALIZE_TEMPLATE_TEST(PackUnpack, Vector, 0) {
    SpecializedRecordTable<0> recordTable;

    std::vector<RamDomain> toPack(0);

    RamDomain tupleRef = recordTable.pack(toPack.data(), 0);

    // empty record has reference 1
    EXPECT_EQ(tupleRef, 1);
    const RamDomain* unpacked{recordTable.unpack(tupleRef, 0)};

    // unpacking empty record returns nullptr
    EXPECT_EQ(unpacked, nullptr);
}

INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 0);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 1);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 2);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 3);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 4);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 5);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 6);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 7);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 11);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 23);
INSTANTIATE_TEMPLATE_TEST(PackUnpack, Vector, 59);

}  // namespace souffle::test
