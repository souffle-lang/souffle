/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file binary_relation_test.cpp
 *
 * A test case testing the binary relation member functions
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/RamTypes.h"
#include "souffle/utility/ContainerUtil.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <cstddef>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "souffle/datastructure/EquivalenceRelation.h"

namespace souffle {
namespace test {

TEST(EqRelTest, Scoping) {
    // simply test that namespaces were setup correctly
    souffle::EquivalenceRelation<souffle::Tuple<RamDomain, 2>> br;
}

using EqRel = souffle::EquivalenceRelation<Tuple<RamDomain, 2>>;

TEST(EqRelTest, Basic) {
    EqRel br;
    // empty bin rel should be exactly that
    EXPECT_EQ(br.size(), 0);
    EXPECT_FALSE(br.contains(1, 2));
    EXPECT_FALSE(br.contains(0, 0));

    // test implicit rules
    br.insert(1, 2);
    EXPECT_EQ(br.size(), 4);
    EXPECT_TRUE(br.contains(1, 2));
    EXPECT_TRUE(br.contains(2, 1));
    EXPECT_TRUE(br.contains(1, 1));
    EXPECT_TRUE(br.contains(2, 2));

    // test insert of exactly one pair
    br.insert(3, 3);
    EXPECT_EQ(br.size(), 5);
    EXPECT_TRUE(br.contains(3, 3));
    EXPECT_FALSE(br.contains(1, 3));
    EXPECT_FALSE(br.contains(3, 2));
    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(EqRelTest, Clear) {
    EqRel br;
    br.insert(0, 44);
    br.insert(0, 1);

    EXPECT_EQ(9, br.size());
    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
    br.clear();
    EXPECT_EQ(0, br.size());
    count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(EqRelTest, Duplicates) {
    EqRel br;
    // test inserting same pair
    for (int i = 0; i < 10; ++i) {
        br.insert(0, 0);
    }
    EXPECT_EQ(br.size(), 1);

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(br.contains(0, 0));
    }
    EXPECT_EQ(br.size(), 1);
    EXPECT_FALSE(br.contains(1, 1));

    // check iteration of duplicate is fine
    Tuple<RamDomain, 2> tup;
    tup[0] = 0;
    tup[1] = 0;
    auto x = br.begin();
    EXPECT_EQ(tup, *x);
    ++x;
    EXPECT_EQ(x, br.end());
}

TEST(EqRelTest, TransitivityTest) {
    // test (a,b) && (b, c) => (a,c) etc
    EqRel br;
    br.insert(1, 2);
    br.insert(2, 3);
    EXPECT_EQ(br.size(), 9);
    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
    EXPECT_TRUE(br.contains(1, 1));
    EXPECT_TRUE(br.contains(1, 2));
    EXPECT_TRUE(br.contains(1, 3));
    EXPECT_TRUE(br.contains(2, 1));
    EXPECT_TRUE(br.contains(2, 2));
    EXPECT_TRUE(br.contains(2, 3));
    EXPECT_TRUE(br.contains(3, 1));
    EXPECT_TRUE(br.contains(3, 2));
    EXPECT_TRUE(br.contains(3, 3));
}

TEST(EqRelTest, PairwiseIncremental) {
    EqRel br;

    const std::size_t N = 100;
    // test inserting ascending pairs still isolates them
    for (std::size_t i = 1; i < N; ++i) {
        br.insert(i, i);
        EXPECT_TRUE(br.contains(i, i));
        br.insert(i + (N + 1), i);
        EXPECT_TRUE(br.contains(i, i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i));
    }
    EXPECT_EQ(br.size(), (N - 1) * 4);
    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(EqRelTest, PairwiseDecremental) {
    EqRel br;

    const std::size_t N = 100;
    // test inserting descending pairs still isolates them
    for (std::size_t i = N; i > 1; --i) {
        br.insert(i, i);
        EXPECT_TRUE(br.contains(i, i));
        br.insert(i + (N + 1), i);
        EXPECT_TRUE(br.contains(i, i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i));
    }

    EXPECT_EQ(br.size(), (N - 1) * 4);
    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(EqRelTest, Shuffled) {
    EqRel br;

    std::size_t N = 100;
    // test inserting data "out of order" keeps isolation
    std::vector<std::size_t> data;
    for (std::size_t i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(data.begin(), data.end(), generator);

    for (auto x : data) {
        br.insert(x, x);
    }

    for (std::size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(br.contains(i, i));
    }

    EXPECT_EQ(br.size(), N);

    // always check the iterator for size too
    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(EqRelTest, MergeExtend) {
    // test running extend for a relation

    // br is {{0,1,2,3,4,5,6}, {8,9}, {44, 70}, {11}}
    EqRel br;

    br.insert(0, 1);
    br.insert(0, 2);
    br.insert(0, 3);
    br.insert(0, 4);
    br.insert(0, 5);
    br.insert(0, 6);

    br.insert(8, 9);

    br.insert(44, 70);

    br.insert(11, 11);
    EXPECT_EQ(br.size(), (7 * 7) + (2 * 2) + (2 * 2) + (1 * 1));

    // br2 is {{0,8,33,99}, {68,69,70}, {101, 102}}
    EqRel br2;
    br2.insert(33, 8);
    br2.insert(33, 99);
    br2.insert(33, 0);

    br2.insert(69, 68);
    br2.insert(69, 70);

    br2.insert(101, 102);
    EXPECT_EQ(br2.size(), (4 * 4) + (3 * 3) + (2 * 2));

    // let's say br2 the new knowledge, so we must extend it to actually contain the implied knowledge from
    // new
    br2.extendAndInsert(br);

    // it should contain {0,1,2,3,4,5,6,8,9,33,99}, {44, 68, 69, 70}, {101, 102}
    // shouldn't contain {11}.
    EXPECT_TRUE(br2.contains(0, 0));
    EXPECT_TRUE(br2.contains(0, 1));
    EXPECT_TRUE(br2.contains(1, 8));
    EXPECT_TRUE(br2.contains(9, 4));
    EXPECT_TRUE(br2.contains(33, 4));
    EXPECT_TRUE(br2.contains(33, 99));

    EXPECT_TRUE(br2.contains(68, 69));
    EXPECT_TRUE(br2.contains(70, 44));

    EXPECT_FALSE(br2.contains(11, 11));

    EXPECT_FALSE(br2.contains(0, 69));
    EXPECT_TRUE(br2.contains(101, 102));

    // check that the new knowledge was inserted in the old relation
    // br is {{0,1,2,3,4,5,6,8,9,33,99},{44,68,69,70},{101,102},{11}}
    EXPECT_EQ(br.size(), (11 * 11) + (4 * 4) + (2 * 2) + (1 * 1));
    // check that it was properly extended
    EXPECT_EQ(br2.size(), (11 * 11) + (4 * 4) + (2 * 2));
}

TEST(EqRelTest, Merge) {
    // test insertAll isolates data
    EqRel br;

    int N = 100;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(data.begin(), data.end(), generator);

    for (int i = 0; i < N; i++) {
        br.insert(data[i], data[i]);
    }

    // also insert a joint pair
    br.insert(N - 1, N + 1);

    EXPECT_EQ((std::size_t)N + 3, br.size());

    EqRel br2;
    EXPECT_EQ(0, br2.size());

    std::size_t count = 0;
    for (auto x : br2) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br2.size());

    br2.insertAll(br);
    EXPECT_EQ((std::size_t)N + 3, br2.size());
    EXPECT_EQ((std::size_t)N + 3, br.size());
    count = 0;
    for (auto x : br2) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br2.size());

    br.clear();
    EXPECT_EQ((std::size_t)N + 3, br2.size());
    EXPECT_EQ(0, br.size());
    EXPECT_FALSE(br.begin() != br.end());

    count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br.size());

    br2.clear();
    EXPECT_EQ(0, br2.size());
    EXPECT_EQ(0, br.size());

    count = 0;
    for (auto x : br2) {
        ++count;
        testutil::ignore(x);
    }
    EXPECT_EQ(count, br2.size());
}

TEST(EqRelTest, IterEmpty) {
    // test iterating over an empty binrel fails
    EqRel br;
    for (auto x : br) {
        EXPECT_FALSE(true);
        testutil::ignore(x);
    }
    EXPECT_EQ(0, br.size());
}

TEST(EqRelTest, IterBasic) {
    EqRel br;
    br.insert(0, 0);
    br.insert(1, 1);
    br.insert(2, 2);

    std::size_t count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }

    EXPECT_EQ(count, br.size());
    // merge one disjoint set
    br.insert(0, 1);
    count = 0;
    for (auto x : br) {
        ++count;
        testutil::ignore(x);
    }

    EXPECT_EQ(count, br.size());
}

TEST(EqRelTest, IterRange) {
    // write some tests to use that templated range for different indexes too
    EqRel br;
    br.insert(0, 1);
    br.insert(0, 2);
    br.insert(0, 3);
    br.insert(0, 4);
    br.insert(0, 5);
    br.insert(0, 6);

    br.insert(8, 9);
    br.insert(8, 10);

    // this should return an iterator covering (3, 0), (3, 1), ..., (3, 6), it's a (3, *) iterator
    auto rangeIt = br.getBoundaries<1>({{3, 18293018}});

    std::vector<std::size_t> posteriorsCovered;
    for (auto tup : rangeIt) {
        posteriorsCovered.push_back(tup[1]);
    }
    EXPECT_EQ(posteriorsCovered.size(), 7);

    // this should be of everything, so doesn't matter the args
    rangeIt = br.getBoundaries<0>({{332, 888}});
    posteriorsCovered.clear();
    for (auto tup : rangeIt) {
        posteriorsCovered.push_back(tup[1]);
    }
    EXPECT_EQ(posteriorsCovered.size(), (7 * 7) + (3 * 3));

    // and now iterate over two levels (exactly one pretty much)
    rangeIt = br.getBoundaries<2>({{2, 3}});
    posteriorsCovered.clear();
    for (auto tup : rangeIt) {
        posteriorsCovered.push_back(tup[1]);
    }
    EXPECT_EQ(posteriorsCovered.size(), 1);
    EXPECT_EQ(posteriorsCovered.front(), 3);

    // and now the same, but for levels 1 and two, stuff that doesn't exist
    rangeIt = br.getBoundaries<1>({{99, 99}});
    posteriorsCovered.clear();
    for (auto tup : rangeIt) {
        posteriorsCovered.push_back(tup[1]);
    }
    EXPECT_EQ(posteriorsCovered.size(), 0);

    rangeIt = br.getBoundaries<2>({{8, 1}});
    posteriorsCovered.clear();
    for (auto tup : rangeIt) {
        posteriorsCovered.push_back(tup[1]);
    }
    EXPECT_EQ(posteriorsCovered.size(), 0);

    // and now the same for an empty binary relation
    br.clear();
    // repeat the same, but notice that we expect the size to be 0 always
    {
        auto rangeIt = br.getBoundaries<1>({{3, 18293018}});

        std::vector<std::size_t> posteriorsCovered;
        for (auto tup : rangeIt) {
            posteriorsCovered.push_back(tup[1]);
        }
        EXPECT_EQ(posteriorsCovered.size(), 0);

        // this should be of everything, so doesn't matter the args
        rangeIt = br.getBoundaries<0>({{332, 888}});
        posteriorsCovered.clear();
        for (auto tup : rangeIt) {
            posteriorsCovered.push_back(tup[1]);
        }
        EXPECT_EQ(posteriorsCovered.size(), 0);

        // and now iterate over two levels (exactly one pretty much)
        rangeIt = br.getBoundaries<2>({{2, 3}});
        posteriorsCovered.clear();
        for (auto tup : rangeIt) {
            posteriorsCovered.push_back(tup[1]);
        }
        EXPECT_EQ(posteriorsCovered.size(), 0);

        // and now the same, but for levels 1 and two, stuff that doesn't exist
        rangeIt = br.getBoundaries<1>({{99, 99}});
        posteriorsCovered.clear();
        for (auto tup : rangeIt) {
            posteriorsCovered.push_back(tup[1]);
        }
        EXPECT_EQ(posteriorsCovered.size(), 0);

        rangeIt = br.getBoundaries<2>({{8, 1}});
        posteriorsCovered.clear();
        for (auto tup : rangeIt) {
            posteriorsCovered.push_back(tup[1]);
        }
        EXPECT_EQ(posteriorsCovered.size(), 0);
    }
}

TEST(EqRelTest, IterPartition) {
    // test that the union equals the input

    // test single set binary rel
    EqRel br;
    std::vector<std::pair<RamDomain, RamDomain>> values;
    RamDomain N = 1000;
    for (RamDomain i = 0; i < N; ++i) {
        br.insert(i, i + 1);
    }

    EXPECT_EQ(std::size_t((N + 1) * (N + 1)), br.size());

    auto chunks = br.partition(400);
    // we can't make too many assumptions..
    EXPECT_TRUE(chunks.size() > 0);

    for (auto chunk : chunks) {
        for (auto x : chunk) {
            values.push_back(std::make_pair(x[0], x[1]));
        }
    }

    EXPECT_EQ(br.size(), values.size());

    br.clear();
    values.clear();
    chunks.clear();

    // many disjoint sets (note, can't use N, because even & odd numbers don't work the same..)
    for (RamDomain i = 0; i < 1000; i += 2) {
        br.insert(i, i + 1);
    }
    EXPECT_EQ((std::size_t)4 * 1000 / 2, br.size());

    chunks = br.partition(400);
    for (auto chunk : chunks) {
        for (auto x : chunk) {
            values.push_back(std::make_pair(x[0], x[1]));
        }
    }

    EXPECT_EQ(br.size(), values.size());
}

TEST(EqRelTest, Scaling) {
    const int N = 100;

    EqRel br;
    for (int i = 0; i < N; ++i) {
        br.insert(i, i);
    }

    EXPECT_EQ(N, br.size());
}

#ifdef _OPENMP
TEST(EqRelTest, ParallelScaling) {
    // use OpenMP this time

    // test with varying number of threads (100000 will likely catch a race condition)
    const int N = 100000;
    std::vector<int> data1;
    std::vector<int> data2;
    for (int i = 0; i < N; ++i)
        data1.push_back(i);
    for (int i = 0; i < N; ++i)
        data2.push_back(i);

    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(data1.begin(), data1.end(), generator);
    shuffle(data2.begin(), data2.end(), generator);

    std::cout << "number of threads: " << omp_get_max_threads() << std::endl;

    EqRel br;
#pragma omp parallel for
    for (int i = 0; i < N; i++) {
        // unfortunately, we can't do insert(data1, data2) as we won't know how many pairs...
        br.insert(data1[i], data1[i]);
        br.insert(data2[i], data2[i]);
    }

    EXPECT_EQ(N, br.size());
    if (N != br.size()) {
        throw std::runtime_error("here's a gdb trap");
    }
}
#endif

}  // namespace test
}  // namespace souffle
