/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ram_type_conversion_test.cpp
 *
 * Tests souffle's type conversions.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/RamTypes.h"
#include <random>
#include <string>

namespace souffle::test {

#define NUMBER_OF_CONVERSION_TESTS 100

TEST(randomConversions, RamSigned) {
    std::mt19937 randomGenerator(3);
    std::uniform_int_distribution<RamDomain> dist(-100, 100);

    for (int i = 0; i < NUMBER_OF_CONVERSION_TESTS; ++i) {
        RamDomain randomNumber = dist(randomGenerator);

        // Test float casting
        EXPECT_EQ(randomNumber, ramBitCast(ramBitCast<RamFloat>(randomNumber)));

        // Test signed casting
        EXPECT_EQ(randomNumber, ramBitCast(ramBitCast<RamUnsigned>(randomNumber)));
    }
}

TEST(randomConversions, RamUnsigned) {
    std::mt19937 randomGenerator(3);
    std::uniform_int_distribution<RamUnsigned> dist(0, 1000);

    for (int i = 0; i < NUMBER_OF_CONVERSION_TESTS; ++i) {
        RamUnsigned randomNumber = dist(randomGenerator);

        // Test float casting
        EXPECT_EQ(randomNumber, ramBitCast<RamUnsigned>(ramBitCast<RamFloat>(randomNumber)));

        // Test signed casting
        EXPECT_EQ(randomNumber, ramBitCast<RamUnsigned>(ramBitCast<RamDomain>(randomNumber)));
    }
}

TEST(randomConversions, RamFloat) {
    std::mt19937 randomGenerator(3);
    std::uniform_real_distribution<RamFloat> dist(-100.0, 100.0);

    for (int i = 0; i < NUMBER_OF_CONVERSION_TESTS; ++i) {
        RamFloat randomNumber = dist(randomGenerator);

        // Test signed casting
        EXPECT_EQ(randomNumber, ramBitCast<RamFloat>(ramBitCast<RamDomain>(randomNumber)));

        // Test unsigned casting
        EXPECT_EQ(randomNumber, ramBitCast<RamFloat>(ramBitCast<RamUnsigned>(randomNumber)));
    }
}

}  // namespace souffle::test
