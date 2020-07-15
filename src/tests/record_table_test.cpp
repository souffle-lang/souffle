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

#include "souffle/CompiledTuple.h"
#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
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
    RecordTable recordTable;
    const Tuple<RamDomain, 3> tuple = {{1, 2, 3}};

    RamDomain ref = pack(recordTable, tuple);

    const RamDomain* ptr = recordTable.unpack(ref, 3);

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(tuple[i], ptr[i]);
    }
}

// Generate random tuples
// pack them all
// unpack and test for equality
TEST(PackUnpack, Tuple) {
    constexpr size_t tupleSize = 3;
    using tupleType = Tuple<RamDomain, tupleSize>;

    RecordTable recordTable;

    // Setup random number generation
    std::default_random_engine randomGenerator(3);
    std::uniform_int_distribution<RamDomain> distribution(
            std::numeric_limits<RamDomain>::lowest(), std::numeric_limits<RamDomain>::max());

    auto random = std::bind(distribution, randomGenerator);

    // Tuples that will be packed
    std::vector<tupleType> toPack(NUMBER_OF_TESTS);

    // Tuple reference after they are packed.
    std::vector<RamDomain> tupleRef(NUMBER_OF_TESTS);

    // Generate and pack the tuples
    for (size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        toPack[i] = {{random(), random(), random()}};
        tupleRef[i] = pack(recordTable, toPack[i]);
    }

    // unpack and test
    for (size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        auto unpacked = recordTable.unpack(tupleRef[i], tupleSize);
        tupleType cmp = {unpacked[0], unpacked[1], unpacked[2]};
        EXPECT_EQ(toPack[i], cmp);
    }
}

// Generate random vectors
// pack them all
// unpack and test for equality
TEST(PackUnpack, Vector) {
    constexpr size_t vectorSize = 10;

    RecordTable recordTable;

    // Tuples that will be packed
    std::vector<std::vector<RamDomain>> toPack(NUMBER_OF_TESTS);

    // Tuple reference after they are packed.
    std::vector<RamDomain> tupleRef(NUMBER_OF_TESTS);

    // Generate and pack the tuples
    for (size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        toPack[i] = testutil::generateRandomVector<RamDomain>(10);
        tupleRef[i] = recordTable.pack(toPack[i].data(), vectorSize);
        std::cerr << "Ref: " << tupleRef[i] << std::endl;
    }

    // unpack and test
    for (size_t i = 0; i < NUMBER_OF_TESTS; ++i) {
        const RamDomain* unpacked{recordTable.unpack(tupleRef[i], vectorSize)};
        for (size_t j = 0; j < vectorSize; ++j) {
            EXPECT_EQ(toPack[i][j], unpacked[j]);
        }
    }
}

}  // namespace souffle::test
