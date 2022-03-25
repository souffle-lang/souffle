/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file flyweight_test.cpp
 *
 * Test (concurrent) flyweights.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/datastructure/ConcurrentFlyweight.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// How many times to repeat each randomized test
#define RANDOM_TESTS 32
#define RANDOM_TEST_SIZE 64

namespace souffle::test {

// Base class for Find-Or-Insert-Copy tests.
class FindOrInsertCopyBase {
    TestCaseInterface& TestInterface;

    // isolated random generator for a thread
    struct RandomGenerator {
        static constexpr char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
        static constexpr std::size_t max_index = sizeof(charset) - 2;

        // The number 6 was chosen so that the strings are relatively short - both
        // to be light on CPU/memory, and to make collisions possible.
        static constexpr std::size_t max_string_len = 6;

        std::mt19937 rand_engine;
        std::uniform_int_distribution<std::size_t> charset_dist;
        std::uniform_int_distribution<std::size_t> string_len_dist;

        RandomGenerator() : charset_dist(0, max_index), string_len_dist(1, max_string_len) {
            std::random_device rd;
            rand_engine.seed(rd());
        }

        // return a random alphanum character
        std::string random_alphanum_string() {
            const std::size_t len = string_len_dist(rand_engine);
            std::string str(len, 0);
            for (std::size_t i = 0; i < len; ++i) {
                str[i] = charset[charset_dist(rand_engine)];
            }

            return str;
        }
    };

    // random generators for up to 256 threads
    RandomGenerator rand_generators[256];

public:
    explicit FindOrInsertCopyBase(TestCaseInterface& Itf) : TestInterface(Itf) {}

    TestCaseInterface& testInterface() {
        return TestInterface;
    }

    void parametrizedFindOrInsertCopy(
            const std::size_t LaneCount, const std::size_t InitialCapacity, std::size_t MaxThreadCount) {
#ifdef _OPENMP
        omp_set_num_threads(static_cast<int>(std::min(MaxThreadCount, (std::size_t)omp_get_max_threads())));
#else
        testutil::ignore(MaxThreadCount);
#endif
        for (int i = 0; i < RANDOM_TESTS; ++i) {
            FlyweightImpl<std::string> flyweight{LaneCount, InitialCapacity};

            random_flyweight(flyweight, 0, RANDOM_TEST_SIZE);

#ifdef _OPENMP
#pragma omp parallel
            {
                srand(omp_get_thread_num());
#pragma omp for
#endif
                for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
                    // Guarantee uniqueness by appending something not in the character set
                    // and then the index.
                    std::string s = random_alphanum_string() + "~" + std::to_string(j);
                    FlyweightImpl<std::string>::index_type data1, data2;
                    bool was_new1, was_new2;
                    std::tie(data1, was_new1) = flyweight.findOrInsert(s);
                    std::tie(data2, was_new2) = flyweight.findOrInsert(s);
                    EXPECT_EQ(data1, data2);
                    EXPECT_TRUE(was_new1);
                    EXPECT_FALSE(was_new2);
                }
#ifdef _OPENMP
            }
#endif
        }
    }

    // Construct a random flyweight by performing random API operations
    std::vector<std::pair<FlyweightImpl<std::string>::index_type, std::string>> random_flyweight(
            FlyweightImpl<std::string>& flyweight, std::size_t min_size, std::size_t max_size) {
        assert(min_size < max_size);
        assert(static_cast<std::size_t>(std::numeric_limits<int>::max()) >= max_size);
        std::vector<std::pair<FlyweightImpl<std::string>::index_type, std::string>> values;
        values.reserve(max_size);

        std::mutex mu;
        const auto add_new = [&] {
            FlyweightImpl<std::string>::index_type key;
            const std::string s = random_alphanum_string();
            bool inserted;
            std::tie(key, inserted) = flyweight.findOrInsert(s);
            EXPECT_TRUE(flyweight.weakContains(s));
            if (inserted) {
                const std::lock_guard<std::mutex> lock(mu);
                values.emplace_back(key, s);
            }
        };

#ifdef _OPENMP
#pragma omp parallel
        {
            srand(omp_get_thread_num());
#pragma omp for
#endif
            for (int i = 0; i < static_cast<int>(max_size); ++i) {
                const auto sel = rand() % 3;
                if (sel == 0) {
                    add_new();
                } else if (sel == 1) {
                    std::size_t size;
                    FlyweightImpl<std::string>::index_type expected_idx;
                    const std::string* expected_val;
                    {
                        const std::lock_guard<std::mutex> lock(mu);
                        size = values.size();
                        if (size > 0) {
                            const auto r = rand();
                            const auto& couple = values[r % size];
                            expected_idx = couple.first;
                            expected_val = &couple.second;
                        }
                    }

                    if (size > 0) {
                        const std::string& fetched = flyweight.fetch(expected_idx);
                        EXPECT_EQ(*expected_val, fetched);
                    } else {
                        add_new();
                    }
                } else {
                    const std::string* expected_val;
                    std::size_t size;
                    {
                        const std::lock_guard<std::mutex> lock(mu);
                        size = values.size();
                        if (size > 0) {
                            const auto r = rand();
                            expected_val = &values[r % size].second;
                        }
                    }

                    if (size > 0) {
                        const bool found = flyweight.weakContains(*expected_val);
                        EXPECT_TRUE(found);
                    } else {
                        add_new();
                    }
                }
            }
#ifdef _OPENMP
        }
#endif

        while (values.size() < min_size) {
            add_new();
        }
        check(flyweight);
        return values;
    }

    void check_iter_contains(FlyweightImpl<std::string>& flyweight) {
        for (const auto& it : flyweight) {
            EXPECT_TRUE(flyweight.weakContains(it.first));
        }
    }

    void check_iter_find(FlyweightImpl<std::string>& flyweight) {
        for (const auto& it : flyweight) {
            bool was_new;
            std::tie(std::ignore, was_new) = flyweight.findOrInsert(it.first);
            EXPECT_TRUE(!was_new);
        }
    }

    // Check invariants
    void check(FlyweightImpl<std::string>& flyweight) {
        check_iter_contains(flyweight);
        check_iter_find(flyweight);
    }

    std::string random_alphanum_string() {
        const std::size_t Tid =
#ifdef _OPENMP
                static_cast<std::size_t>(omp_get_thread_num());
#else
                0;
#endif
        return rand_generators[Tid].random_alphanum_string();
    }
};

// Test with a single access lane and a single thread
DERIVED_TEST(Flyweight, FindOrInsertCopy_OneLane_OneThread, FindOrInsertCopyBase) {
    parametrizedFindOrInsertCopy(1, 1, 1);
}

// Test with two access lanes and one thread
DERIVED_TEST(Flyweight, FindOrInsertCopy_TwoLanes_OneThread, FindOrInsertCopyBase) {
    parametrizedFindOrInsertCopy(2, 1, 1);
}

#ifdef _OPENMP
// Test with a single access lane and as many threads
DERIVED_TEST(Flyweight, FindOrInsertCopy_OneLane_ManyThreads, FindOrInsertCopyBase) {
    const std::size_t threadCount = static_cast<std::size_t>(omp_get_max_threads());
    parametrizedFindOrInsertCopy(1, 1, threadCount);
}

// Test with two access lanes and many threads
DERIVED_TEST(Flyweight, FindOrInsertCopy_TwoLanes_ManyThreads, FindOrInsertCopyBase) {
    const std::size_t threadCount = static_cast<std::size_t>(omp_get_max_threads());
    parametrizedFindOrInsertCopy(2, 1, threadCount);
}

// Test with one access lane per thread
DERIVED_TEST(Flyweight, FindOrInsertCopy_OneLanePerThread_ManyThreads, FindOrInsertCopyBase) {
    const std::size_t threadCount = static_cast<std::size_t>(omp_get_max_threads());
    parametrizedFindOrInsertCopy(threadCount, 1, threadCount);
}

// Test with one access lane per thread, high initial capacity that amortizes growing
DERIVED_TEST(Flyweight, FindOrInsertCopy_OneLanePerThread_ManyThreads_HighCapacity, FindOrInsertCopyBase) {
    const std::size_t threadCount = static_cast<std::size_t>(omp_get_max_threads());
    parametrizedFindOrInsertCopy(threadCount, 1UL << 16, threadCount);
}
#endif

TEST(Flyweight, IteratorEmptyBeginEnd) {
    FlyweightImpl<std::string> flyweight{1};
    EXPECT_EQ(flyweight.begin(), flyweight.begin());
    EXPECT_EQ(flyweight.end(), flyweight.end());
    EXPECT_EQ(flyweight.begin()++, flyweight.end());
    for (const auto& val : flyweight) {
        testutil::ignore(val);
        EXPECT_TRUE(false);
    }
}

}  // namespace souffle::test
