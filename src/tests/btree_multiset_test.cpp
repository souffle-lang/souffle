/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file btree_multiset_test.cpp
 *
 * A test case testing the B-trees utilization as multisets.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/datastructure/BTree.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace std {

template <typename A, typename B>
struct hash<tuple<A, B>> {
    std::size_t operator()(const tuple<A, B>& t) const {
        auto a = std::hash<A>()(get<0>(t));
        auto b = std::hash<B>()(get<1>(t));
        // from http://www.boost.org/doc/libs/1_35_0/doc/html/boost/hash_combine_id241013.html
        return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
    }
};

template <typename A, typename B>
std::ostream& operator<<(std::ostream& out, const tuple<A, B>& t) {
    return out << "[" << get<0>(t) << "," << get<1>(t) << "]";
}
}  // namespace std

namespace souffle {

namespace test {

TEST(BTreeMultiSet, Basic) {
    const bool DEBUG = false;

    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    EXPECT_EQ(3, test_set::max_keys_per_node);

    // check initial conditions
    EXPECT_EQ(0u, t.size());
    EXPECT_FALSE(t.contains(10));
    EXPECT_FALSE(t.contains(12));
    EXPECT_FALSE(t.contains(14));
    EXPECT_EQ(0, t.getDepth());
    EXPECT_EQ(0, t.getNumNodes());

    if (DEBUG) t.printTree();

    // add an element

    t.insert(12);
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }

    EXPECT_EQ(1u, t.size());
    EXPECT_FALSE(t.contains(10));
    EXPECT_TRUE(t.contains(12));
    EXPECT_FALSE(t.contains(14));
    EXPECT_EQ(1, t.getDepth());
    EXPECT_EQ(1, t.getNumNodes());

    // add a larger element

    t.insert(14);
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }
    EXPECT_EQ(2u, t.size());
    EXPECT_FALSE(t.contains(10));
    EXPECT_TRUE(t.contains(12));
    EXPECT_TRUE(t.contains(14));
    EXPECT_EQ(1, t.getDepth());
    EXPECT_EQ(1, t.getNumNodes());

    // add a smaller element

    t.insert(10);
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }
    EXPECT_EQ(3u, t.size());
    EXPECT_TRUE(t.contains(10));
    EXPECT_TRUE(t.contains(12));
    EXPECT_TRUE(t.contains(14));
    EXPECT_EQ(1, t.getDepth());
    EXPECT_EQ(1, t.getNumNodes());

    // cause a split
    t.insert(11);
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }
    EXPECT_EQ(4u, t.size());
    EXPECT_TRUE(t.contains(10));
    EXPECT_TRUE(t.contains(11));
    EXPECT_TRUE(t.contains(12));
    EXPECT_TRUE(t.contains(14));

    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }

    t.insert(12);
    EXPECT_EQ(5u, t.size());
    t.insert(12);
    EXPECT_EQ(6u, t.size());
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }

    t.insert(15);
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }

    t.insert(16);
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }
}

TEST(BTreeMultiSet, Duplicates) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;
    for (int i = 0; i < 10; i++) {
        t.insert(0);
    }
    EXPECT_EQ(10, t.size());
    std::vector<int> data(t.begin(), t.end());
    EXPECT_EQ(10, data.size());
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, data[i]);
    }
}

TEST(BTreeMultiSet, Incremental) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;
    int N = 1000;
    for (int i = 0; i < N; i++) {
        t.insert(i);
        for (int j = 0; j < N; j++) {
            EXPECT_EQ(j <= i, t.contains(j)) << "i=" << i << ", j=" << j;
        }
    }
}

TEST(BTreeMultiSet, Decremental) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;
    int N = 1000;
    for (int i = N; i >= 0; i--) {
        t.insert(i);
        for (int j = 0; j < N; j++) {
            EXPECT_EQ(j >= i, t.contains(j)) << "i=" << i << ", j=" << j;
        }
    }
}

TEST(BTreeMultiSet, Shuffled) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    int N = 10000;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(data.begin(), data.end(), generator);

    for (int i = 0; i < N; i++) {
        t.insert(data[i]);
    }

    for (int i = 0; i < N; i++) {
        EXPECT_TRUE(t.contains(i)) << "i=" << i;
    }
}

TEST(BTreeMultiSet, IteratorEmpty) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;

    EXPECT_EQ(t.begin(), t.end());
}

TEST(BTreeMultiSet, IteratorBasic) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;

    for (int i = 0; i < 10; i++) {
        t.insert(i);
    }

    //        t.printTree();

    auto it = t.begin();
    auto e = t.end();

    EXPECT_NE(it, e);

    int last = -1;
    for (int i : t) {
        EXPECT_EQ(last + 1, i);
        last = i;
    }
    EXPECT_EQ(last, 9);
}

TEST(BTreeMultiSet, IteratorStress) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    int N = 1000;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(data.begin(), data.end(), generator);

    for (int i = 0; i < N; i++) {
        EXPECT_EQ((std::size_t)i, t.size());

        int last = -1;
        for (int i : t) {
            EXPECT_LT(last, i);
            last = i;
        }

        t.insert(data[i]);
    }
}

TEST(BTreeMultiSet, BoundaryTest) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    for (int i = 0; i < 10; i++) {
        t.insert(i);
    }

    //        t.printTree();

    auto a = t.lower_bound(5);
    EXPECT_EQ(5, *a);

    auto b = t.upper_bound(5);
    EXPECT_EQ(6, *b);

    // add duplicates

    t.insert(5);
    t.insert(5);
    t.insert(5);

    //        t.printTree(); std::cout << "\n";

    // test again ..
    a = t.lower_bound(5);
    EXPECT_EQ(5, *a);

    b = t.upper_bound(5);
    EXPECT_EQ(6, *b);

    // check the distance
    EXPECT_EQ(5, *a);
    ++a;
    EXPECT_EQ(5, *a);
    ++a;
    EXPECT_EQ(5, *a);
    ++a;
    EXPECT_EQ(5, *a);
    ++a;
    EXPECT_EQ(6, *a);
}

TEST(BTreeMultiSet, BoundaryEmpty) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    EXPECT_EQ(t.end(), t.lower_bound(5));
    EXPECT_EQ(t.end(), t.upper_bound(5));

    t.insert(4);

    EXPECT_EQ(t.lower_bound(3), t.upper_bound(3));
    EXPECT_EQ(t.lower_bound(5), t.upper_bound(5));

    t.insert(6);
    EXPECT_EQ(t.lower_bound(3), t.upper_bound(3));
    EXPECT_EQ(t.lower_bound(5), t.upper_bound(5));

    t.insert(5);

    EXPECT_EQ(t.lower_bound(3), t.upper_bound(3));
    EXPECT_NE(t.lower_bound(5), t.upper_bound(5));
}

TEST(BTreeMultiSet, Load) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    for (int N = 0; N < 100; N++) {
        // generate some ordered data
        std::vector<int> data;

        for (int i = 0; i < N; i++) {
            data.push_back(i);
        }
        auto t = test_set::load(data.begin(), data.end());
        EXPECT_EQ(data.size(), t.size());
        EXPECT_TRUE(t.check());
        int last = -1;
        for (int c : t) {
            EXPECT_EQ(last + 1, c);
            last = c;
        }
        EXPECT_EQ(last, N - 1);
    }
}

TEST(BTreeMultiSet, Clear) {
    using test_set = btree_multiset<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    EXPECT_TRUE(t.empty());

    t.insert(5);

    EXPECT_FALSE(t.empty());
    t.clear();
    EXPECT_TRUE(t.empty());

    t.clear();
    EXPECT_TRUE(t.empty());
}

using Entry = std::tuple<int, int64_t>;

std::vector<Entry> getData(unsigned numEntries) {
    std::vector<Entry> res(numEntries);
    int k = 0;
    for (unsigned i = 0; i < numEntries; i++) {
        res[k++] = Entry(i / 100, i % 100);
    }
    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(res.begin(), res.end(), generator);
    return res;
}

using time_point = std::chrono::high_resolution_clock::time_point;

time_point now() {
    return std::chrono::high_resolution_clock::now();
}

int64_t duration(const time_point& start, const time_point& end) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

template <typename Op>
int64_t time(const std::string& name, const Op& operation) {
    std::cout << "\t" << std::setw(30) << std::setiosflags(std::ios::left) << name
              << std::resetiosflags(std::ios::left) << " ... " << std::flush;
    auto a = now();
    operation();
    auto b = now();
    auto time = duration(a, b);
    std::cout << " done [" << std::setw(5) << time << "ms]\n";
    return time;
}

template <typename C>
struct reserver {
    void operator()(C&, std::size_t) const {
        // default: no action
    }
};

template <typename A, typename B, typename C, typename D>
struct reserver<std::unordered_set<A, B, C, D>> {
    void operator()(std::unordered_set<A, B, C, D>& set, std::size_t size) const {
        set.reserve(size);
    }
};

#define checkPerformance(set_type, name, in, out)                                      \
    {                                                                                  \
        std::cout << "Testing: " << name << " ..\n";                                   \
        set_type set;                                                                  \
        time("filling set", [&]() {                                                    \
            reserver<set_type>()(set, in.size());                                      \
            for (const auto& cur : in) {                                               \
                set.insert(cur);                                                       \
            }                                                                          \
        });                                                                            \
        int counter = 0;                                                               \
        time("full scan", [&]() {                                                      \
            for (auto it = set.begin(); it != set.end(); ++it) {                       \
                counter++;                                                             \
            }                                                                          \
        });                                                                            \
        EXPECT_EQ((std::size_t)counter, set.size());                                   \
        bool allPresent = true;                                                        \
        time("membership in", [&]() {                                                  \
            for (const auto& cur : in) {                                               \
                allPresent = (set.find(cur) != set.end()) && allPresent;               \
            }                                                                          \
        });                                                                            \
        EXPECT_TRUE(allPresent);                                                       \
        bool allMissing = true;                                                        \
        time("membership out", [&]() {                                                 \
            for (const auto& cur : out) {                                              \
                allMissing = (set.find(cur) == set.end()) && allMissing;               \
            }                                                                          \
        });                                                                            \
        EXPECT_TRUE(allMissing);                                                       \
        bool allFound = true;                                                          \
        time("lower_boundaries", [&]() {                                               \
            for (const auto& cur : in) {                                               \
                allFound = (set.lower_bound(cur) == set.find(cur)) && allFound;        \
            }                                                                          \
        });                                                                            \
        EXPECT_TRUE(allFound);                                                         \
        allFound = true;                                                               \
        time("upper_boundaries", [&]() {                                               \
            for (const auto& cur : in) {                                               \
                allFound = (set.upper_bound(cur) == (++set.find(cur))) && allFound;    \
            }                                                                          \
        });                                                                            \
        EXPECT_TRUE(allFound);                                                         \
        allFound = true;                                                               \
        time("boundaries on missing elements", [&]() {                                 \
            for (const auto& cur : out) {                                              \
                allFound = (set.lower_bound(cur) == set.upper_bound(cur)) && allFound; \
            }                                                                          \
        });                                                                            \
        EXPECT_TRUE(allFound);                                                         \
        set_type a(in.begin(), in.end());                                              \
        set_type b(out.begin(), out.end());                                            \
        time("merge two sets", [&]() { a.insert(b.begin(), b.end()); });               \
        std::cout << "\tDone!\n\n";                                                    \
    }

TEST(Performance, Basic) {
    int N = 1 << 18;

    // get list of tuples to be inserted
    std::cout << "Generating Test-Data ...\n";
    std::vector<Entry> in;
    std::vector<Entry> out;
    time("generating data", [&]() {
        auto data = getData(2 * N);
        for (std::size_t i = 0; i < data.size(); i += 2) {
            in.push_back(data[i]);
            out.push_back(data[i + 1]);
        }
    });

    using t1 = std::set<Entry>;
    checkPerformance(t1, " -- warm up -- ", in, out);
    using t2 = btree_multiset<Entry, detail::comparator<Entry>, std::allocator<Entry>, 256,
            detail::linear_search>;
    checkPerformance(t2, "souffle btree_multiset - 256 - linear", in, out);
    using t3 = btree_multiset<Entry, detail::comparator<Entry>, std::allocator<Entry>, 256,
            detail::binary_search>;
    checkPerformance(t3, "souffle btree_multiset - 256 - binary", in, out);
}

TEST(Performance, Load) {
    int N = 1 << 20;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }

    // take time for conventional load
    time("conventional load", [&]() { btree_multiset<int> t(data.begin(), data.end()); });

    // take time for structured load
    time("bulk-load", [&]() { auto t = btree_multiset<int>::load(data.begin(), data.end()); });
}
}  // namespace test
}  // end namespace souffle
