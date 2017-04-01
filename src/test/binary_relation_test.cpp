/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers and/or its affiliates. All rights reserved
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

#include "test.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "BinaryRelation.h"

namespace souffle {
namespace test {

// easy function to suppress unused var warnings (when we REALLY don't need to use them!)
namespace binreltest {
template <class T>
void ignore(const T&) {}
}

TEST(BinRelTest, Scoping) {
    // simply test that namespaces were setup correctly
    souffle::BinaryRelation<souffle::ram::Tuple<RamDomain, 2>> br;
}

typedef BinaryRelation<ram::Tuple<RamDomain, 2>> BinRel;

TEST(BinRelTest, Basic) {
    BinRel br;
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
    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(BinRelTest, Clear) {
    BinRel br;
    br.insert(0, 44);
    br.insert(0, 1);

    EXPECT_EQ(9, br.size());
    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());
    br.clear();
    EXPECT_EQ(0, br.size());
    count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(BinRelTest, Duplicates) {
    BinRel br;
    // test inserting same pair
    for (int i = 0; i < 10; ++i) br.insert(0, 0);
    EXPECT_EQ(br.size(), 1);

    for (int i = 0; i < 10; ++i) EXPECT_TRUE(br.contains(0, 0));
    EXPECT_EQ(br.size(), 1);
    EXPECT_FALSE(br.contains(1, 1));

    // check iteration of duplicate is fine
    ram::Tuple<RamDomain, 2> tup;
    tup[0] = 0;
    tup[1] = 0;
    auto x = br.begin();
    EXPECT_EQ(tup, *x);
    ++x;
    EXPECT_EQ(x, br.end());
}

TEST(BinRelTest, TransitivityTest) {
    // test (a,b) && (b, c) => (a,c) etc
    BinRel br;
    br.insert(1, 2);
    br.insert(2, 3);
    EXPECT_EQ(br.size(), 9);
    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
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

TEST(BinRelTest, PairwiseIncremental) {
    BinRel br;

    const int N = 100;
    // test inserting ascending pairs still isolates them
    for (int i = 1; i < N; ++i) {
        br.insert(i, i);
        EXPECT_TRUE(br.contains(i, i));
        br.insert(i + (N + 1), i);
        EXPECT_TRUE(br.contains(i, i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i));
    }
    EXPECT_EQ(br.size(), (N - 1) * 4);
    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(BinRelTest, PairwiseDecremental) {
    BinRel br;

    const int N = 100;
    // test inserting descending pairs still isolates them
    for (int i = N; i > 1; --i) {
        br.insert(i, i);
        EXPECT_TRUE(br.contains(i, i));
        br.insert(i + (N + 1), i);
        EXPECT_TRUE(br.contains(i, i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i + (N + 1)));
        EXPECT_TRUE(br.contains(i + (N + 1), i));
    }

    EXPECT_EQ(br.size(), (N - 1) * 4);
    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(BinRelTest, Shuffled) {
    BinRel br;

    int N = 100;
    // test inserting data "out of order" keeps isolation
    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_shuffle(data.begin(), data.end());

    for (auto x : data) {
        br.insert(x, x);
    }

    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(br.contains(i, i));
    }

    EXPECT_EQ(br.size(), N);

    // always check the iterator for size too
    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());
}

TEST(BinRelTest, Copy) {
    // test =assign keeps copy independence
    BinRel br;

    int N = 100;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_shuffle(data.begin(), data.end());

    for (int i = 0; i < N; i++) {
        br.insert(data[i], data[i]);
    }

    EXPECT_EQ((size_t)N, br.size());

    for (int i = 0; i < N; i++) {
        ram::Tuple<RamDomain, 2> t;
        t[0] = i;
        t[1] = i;
        EXPECT_TRUE(br.find(t) != br.end()) << "i=" << i;
    }

    BinRel br2;
    EXPECT_EQ(0, br2.size());
    EXPECT_FALSE(br2.contains(0, 0));

    br2 = br;
    EXPECT_EQ((size_t)N, br.size());
    EXPECT_EQ((size_t)N, br2.size());

    for (int i = 0; i < N; ++i) {
        ram::Tuple<RamDomain, 2> t;
        t[0] = i;
        t[1] = i;
        EXPECT_TRUE(br.find(t) != br.end());
        EXPECT_TRUE(br2.find(t) != br2.end());
    }

    // construct a new one an insert into only one
    ram::Tuple<RamDomain, 2> t;
    t[0] = N + 1;
    t[1] = N + 1;
    EXPECT_FALSE(br.find(t) != br.end());
    EXPECT_FALSE(br2.find(t) != br2.end());
    br2.insert(t[0], t[1]);
    EXPECT_FALSE(br.find(t) != br.end());
    EXPECT_TRUE(br2.find(t) != br2.end());
}

// TEST(BinRelTest, CopyScope) {
//     //simply test whether scope is fine in scope changes
//     BinRel br1;
//     {
//         BinRel br2;
//         for (int i = 0; i < 5000; ++i) {
//             br2.insert(i,i);
//         }

//         br1 = br2;
//     }

//     EXPECT_EQ(5000, br1.size());
// }

TEST(BinRelTest, Merge) {
    // test insertAll isolates data
    BinRel br;

    int N = 100;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    random_shuffle(data.begin(), data.end());

    for (int i = 0; i < N; i++) {
        br.insert(data[i], data[i]);
    }

    // also insert a joint pair
    br.insert(N - 1, N + 1);

    EXPECT_EQ((size_t)N + 3, br.size());

    BinRel br2;
    EXPECT_EQ(0, br2.size());

    size_t count = 0;
    for (auto x : br2) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br2.size());

    br2.insertAll(br);
    EXPECT_EQ((size_t)N + 3, br2.size());
    EXPECT_EQ((size_t)N + 3, br.size());
    count = 0;
    for (auto x : br2) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br2.size());

    br.clear();
    EXPECT_EQ((size_t)N + 3, br2.size());
    EXPECT_EQ(0, br.size());
    EXPECT_FALSE(br.begin() != br.end());

    count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br.size());

    br2.clear();
    EXPECT_EQ(0, br2.size());
    EXPECT_EQ(0, br.size());

    count = 0;
    for (auto x : br2) {
        ++count;
        binreltest::ignore(x);
    }
    EXPECT_EQ(count, br2.size());
}

TEST(BinRelTest, IterEmpty) {
    // test iterating over an empty binrel fails
    BinRel br;
    for (auto x : br) {
        EXPECT_FALSE(true);
        binreltest::ignore(x);
    }
    EXPECT_EQ(0, br.size());
}

TEST(BinRelTest, IterBasic) {
    BinRel br;
    br.insert(0, 0);
    br.insert(1, 1);
    br.insert(2, 2);

    size_t count = 0;
    for (auto x : br) {
        EXPECT_EQ(x[0], count);
        EXPECT_EQ(x[1], count);
        ++count;
        binreltest::ignore(x);
    }

    EXPECT_EQ(count, br.size());
    // merge one disjoint set
    br.insert(0, 1);
    count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }

    EXPECT_EQ(count, br.size());
}

TEST(BinRelTest, IterFind) {
    BinRel br;
    ram::Tuple<RamDomain, 2> t;
    t[0] = 0;
    t[1] = 0;

    // find something that doesn't exist in empty br
    for (auto x = br.find(t); x != br.end(); ++x) {
        EXPECT_TRUE(false);
    }

    // make it exist
    br.insert(0, 0);
    size_t count = 0;
    for (auto x = br.find(t); x != br.end(); ++x) {
        ++count;
    }
    EXPECT_EQ(count, br.size());

    // try and find something that doesn't exist in non-empty br
    t[1] = 1;
    for (auto x = br.find(t); x != br.end(); ++x) {
        EXPECT_TRUE(false);
    }
    EXPECT_EQ(1, br.size());
}

TEST(BinRelTest, IterFindBetween) {
    BinRel br;
    br.insert(0, 1);
    br.insert(1, 2);
    br.insert(2, 3);

    // try and perform findBetween on a single
    ram::Tuple<RamDomain, 2> t1;
    t1[0] = 1;
    t1[1] = 0;
    ram::Tuple<RamDomain, 2> t2;
    t2[0] = 1;
    t2[1] = 0;

    size_t count = 0;
    for (auto x = br.findBetween(t1, t2); x != br.end(); ++x) {
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(BinRelTest, IterPartition) {
    // test that the union equals the input

    // test single set binary rel
    BinRel br;
    std::set<std::pair<RamDomain, RamDomain>> values;
    RamDomain N = 1000;
    for (RamDomain i = 0; i < N; ++i) {
        br.insert(i, i + 1);
    }

    EXPECT_EQ((N + 1) * (N + 1), br.size());

    auto chunks = br.partition(400);
    // we can't make too many assumptions..
    EXPECT_TRUE(chunks.size() > 0);

    for (auto chunk : chunks) {
        for (auto x = chunk.begin(); x != chunk.end(); ++x) {
            values.insert(std::make_pair((*x)[0], (*x)[1]));
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
    EXPECT_EQ((size_t)4 * 1000 / 2, br.size());

    chunks = br.partition(400);
    for (auto chunk : chunks) {
        for (auto x = chunk.begin(); x != chunk.end(); ++x) {
            values.insert(std::make_pair((*x)[0], (*x)[1]));
        }
    }
    EXPECT_EQ(br.size(), values.size());
}

TEST(BinRelTest, ParallelTest) {
    // insert a lot of times into a disjoint set over multiple std::threads

    BinRel br;
    std::vector<std::thread> starts;
    // number of inserts per thread
    int N = 1000;
    // int N = 100000;

    starts.push_back(std::thread([&]() {
        for (RamDomain i = 0; i < N * 4; i += 4) br.insert(i, i + 4);
    }));

    starts.push_back(std::thread([&]() {
        for (RamDomain i = 1; i < N * 4; i += 4) br.insert(i, i + 4);
    }));

    starts.push_back(std::thread([&]() {
        for (RamDomain i = 2; i < N * 4; i += 4) br.insert(i, i + 4);
    }));

    starts.push_back(std::thread([&]() {
        for (RamDomain i = 3; i < N * 4; i += 4) br.insert(i, i + 4);
    }));

    for (auto& r : starts) r.join();

    EXPECT_EQ((size_t)(N + 1) * (N + 1) * 4, br.size());

    size_t count = 0;
    for (auto x : br) {
        ++count;
        binreltest::ignore(x);
    }

    EXPECT_EQ(count, br.size());
}

#ifdef _OPENMP
TEST(BinRelTest, ParallelScaling) {
    // use OpenMP this time

    // test with varying number of threads
    // const int N = 1000000;
    const int N = 1000;
    std::vector<int> data1;
    std::vector<int> data2;
    for (int i = 0; i < N; ++i) data1.push_back(i);
    for (int i = 0; i < N; ++i) data2.push_back(i);

    std::random_shuffle(data1.begin(), data1.end());
    std::random_shuffle(data2.begin(), data2.end());

    for (int i = 0; i <= 8; i++) {
        BinRel br;
        omp_set_num_threads(i);

        double start = omp_get_wtime();

#pragma omp parallel
        {
#pragma omp for
            for (int i = 0; i < N; i++) {
                // unfortunately, we can't do insert(data1, data2) as we won't know how many pairs...
                br.insert(data1[i], data1[i]);
                br.insert(data2[i], data2[i]);
            }
        }

        double end = omp_get_wtime();

        std::cout << "Number of threads: " << i << "[" << (end - start) << "ms]\n";

        EXPECT_EQ(N, br.size());
    }
}
#endif
}
}