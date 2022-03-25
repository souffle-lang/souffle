/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file btree_set_test.cpp
 *
 * A test case testing the B-trees utilization as sets.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "souffle/datastructure/BTree.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

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

namespace souffle::test {

TEST(BTreeSet, Basic) {
    const bool DEBUG = false;

    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

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

    EXPECT_EQ(4u, t.size());
    t.insert(12);
    EXPECT_EQ(4u, t.size());
    t.insert(12);
    EXPECT_EQ(4u, t.size());

    t.insert(10);
    EXPECT_EQ(4u, t.size());

    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }

    t.insert(15);
    EXPECT_EQ(5u, t.size());
    EXPECT_EQ(2, t.getDepth());
    EXPECT_EQ(3, t.getNumNodes());
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }

    t.insert(16);
    EXPECT_EQ(6u, t.size());
    if (DEBUG) {
        t.printTree();
        std::cout << "\n";
    }
}

TEST(BTreeSet, Duplicates) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    for (int i = 0; i < 10; i++) {
        t.insert(0);
    }

    EXPECT_EQ(1, t.size());

    EXPECT_EQ(0, *t.begin());

    //        t.printTree();
}

TEST(BTreeSet, Incremental) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    int N = 1000;

    for (int i = 0; i < N; i++) {
        //        std::cout << "\nBefore:\n";
        //        t.printTree();
        //        std::cout << "\n";
        t.insert(i);
        //        std::cout << "\nAfter:\n";
        //        std::cout << "\n";
        //        t.printTree();
        //        std::cout << "\n";
        //        std::cout << "\n\n";

        for (int j = 0; j < N; j++) {
            EXPECT_EQ(j <= i, t.contains(j)) << "i=" << i << ", j=" << j;
        }
    }

    t.printStats();
}

TEST(BTreeSet, Decremental) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    int N = 1000;

    for (int i = N; i >= 0; i--) {
        //            std::cout << "\nBefore:\n";
        //            t.printTree();
        //            std::cout << "\n";
        t.insert(i);
        //            std::cout << "\nAfter:\n";
        //            t.printTree();
        //            std::cout << "\n";
        //            std::cout << "\n\n";

        for (int j = 0; j < N; j++) {
            EXPECT_EQ(j >= i, t.contains(j)) << "i=" << i << ", j=" << j;
        }
    }
}

TEST(BTreeSet, Shuffled) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

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

    //        t.printTree();

    for (int i = 0; i < N; i++) {
        EXPECT_TRUE(t.contains(i)) << "i=" << i;
    }
}

TEST(BTreeSet, Copy) {
    using test_set = btree_set<int>;

    test_set t;

    int N = 100000;

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

    EXPECT_EQ((std::size_t)N, t.size());

    for (int i = 0; i < N; i++) {
        EXPECT_TRUE(t.find(i) != t.end()) << "i=" << i;
    }

    test_set t2;
    EXPECT_EQ((std::size_t)N, t.size());
    EXPECT_EQ(0, t2.size());

    t2 = t;

    EXPECT_EQ((std::size_t)N, t.size());
    EXPECT_EQ((std::size_t)N, t2.size());

    for (int i = 0; i < N; i++) {
        EXPECT_TRUE(t.find(i) != t.end()) << "i=" << i;
    }
    for (int i = 0; i < N; i++) {
        EXPECT_TRUE(t2.find(i) != t2.end()) << "i=" << i;
    }

    EXPECT_FALSE(t.find(N + 1) != t.end());
    EXPECT_FALSE(t2.find(N + 1) != t2.end());
    t2.insert(N + 1);
    EXPECT_FALSE(t.find(N + 1) != t.end());
    EXPECT_TRUE(t2.find(N + 1) != t2.end());

    for (int i = 0; i < N; i++) {
        EXPECT_NE(&*t.find(i), &*t2.find(i)) << "i=" << i;
    }
}

TEST(BTreeSet, Merge) {
    using test_set = btree_set<int>;

    test_set a;
    test_set b;

    a.insert(1);
    a.insert(2);
    a.insert(3);
    a.insert(4);

    b.insert(2);
    b.insert(4);
    b.insert(6);

    EXPECT_NE(a, b);

    test_set c = a;
    test_set d = b;

    EXPECT_NE(c, d);
}

TEST(BTreeSet, IteratorEmpty) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;

    EXPECT_EQ(t.begin(), t.end());
}

TEST(BTreeSet, IteratorBasic) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;
    test_set t;

    int N = 10;

    for (int i = 0; i <= N; i++) {
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
    EXPECT_EQ(last, N);
}

TEST(BTreeSet, IteratorStress) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    int N = 1000;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::random_device rd;
    std::mt19937 generator(rd());

    shuffle(data.begin(), data.end(), generator);

    int max = -1;
    for (int i = 0; i < N; i++) {
        EXPECT_EQ((std::size_t)i, t.size());

        int last = -1;
        for (int i : t) {
            EXPECT_LT(last, i);
            last = i;
        }
        EXPECT_EQ(last, max);

        t.insert(data[i]);
        max = (data[i] > max) ? data[i] : max;
    }
}

TEST(BTreeSet, BoundaryTest) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    for (int i = 0; i < 10; i++) {
        t.insert(i);
    }

    //    t.printTree();

    auto a = t.lower_bound(5);
    EXPECT_EQ(5, *a);

    auto b = t.upper_bound(5);
    EXPECT_EQ(6, *b);

    // add duplicates

    t.insert(5);
    t.insert(5);
    t.insert(5);

    //    t.printTree(); std::cout << "\n";

    // test again ..
    a = t.lower_bound(5);
    EXPECT_EQ(5, *a);

    b = t.upper_bound(5);
    EXPECT_EQ(6, *b);

    // check the distance
    EXPECT_EQ(++a, b);
}

TEST(BTreeSet, BoundaryEmpty) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

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

TEST(BTreeSet, Load) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    for (int N = 0; N < 100; N++) {
        // generate some ordered data
        std::vector<int> data;

        for (int i = 0; i < N; i++) {
            data.push_back(i);
        }

        auto t = test_set::load(data.begin(), data.end());
        //        t.printTree();

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

TEST(BTreeSet, Clear) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    EXPECT_TRUE(t.empty());

    t.insert(5);

    EXPECT_FALSE(t.empty());
    t.clear();
    EXPECT_TRUE(t.empty());

    t.clear();
    EXPECT_TRUE(t.empty());
}

TEST(BTreeSet, ChunkSplit) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    test_set t;

    // TODO: test for empty

    for (int i = 0; i < 100; i++) {
        t.insert(i);
    }

    // split chunks
    auto chunks = t.getChunks(20);

    //        EXPECT_EQ(20, chunks.size());

    for (const auto& cur : chunks) {
        for (int i : cur) {
            std::cout << i << ", ";
        }
        std::cout << "\n";
    }

    int last = -1;
    for (const auto& chunk : chunks) {
        for (auto current : chunk) {
            EXPECT_EQ(last + 1, current);
            last = current;
        }
    }
}

TEST(BTreeSet, ChunkSplitStress) {
    using test_set = btree_set<int, detail::comparator<int>, std::allocator<int>, 16>;

    for (int i = 0; i < 1000; i++) {
        // generate random sequence
        std::vector<int> data;
        for (int j = 0; j < i; j++) {
            data.push_back(j);
        }
        std::random_device rd;
        std::mt19937 generator(rd());

        shuffle(data.begin(), data.end(), generator);

        // fill tree
        test_set t;

        for (int x : data) {
            t.insert(x);
        }

        for (int j = 1; j < 100; j++) {
            auto chunks = t.getChunks(j);
            //                EXPECT_LT(chunks.size(), j * 2);

            if (chunks.empty()) continue;

            // check covered range
            EXPECT_EQ(0, *chunks.front().begin());

            int last = -1;
            for (const auto& chunk : chunks) {
                for (auto current : chunk) {
                    EXPECT_EQ(last + 1, current);
                    last = current;
                }
            }

            EXPECT_EQ(i - 1, last);
        }
    }
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
    int64_t time = duration(a, b);
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
        EXPECT_EQ(in.size(), set.size());                                              \
        int counter = 0;                                                               \
        time("full scan", [&]() {                                                      \
            for (auto it = set.begin(); it != set.end(); ++it) {                       \
                counter++;                                                             \
            }                                                                          \
        });                                                                            \
        EXPECT_EQ(in.size(), (std::size_t)counter);                                    \
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
    //        int N = 1<<22;
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

    using t2 = btree_set<Entry, detail::comparator<Entry>, std::allocator<Entry>, 256, detail::linear_search>;
    checkPerformance(t2, "souffle btree_set - 256 - linear", in, out);

    using t3 = btree_set<Entry, detail::comparator<Entry>, std::allocator<Entry>, 256, detail::binary_search>;
    checkPerformance(t3, "souffle btree_set - 256 - binary", in, out);
}

TEST(Performance, Load) {
    //        int N = 1<<24;
    int N = 1 << 20;

    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }

    // take time for conventional load
    time("conventional load", [&]() { btree_set<int> t(data.begin(), data.end()); });

    // take time for structured load
    time("bulk-load", [&]() { auto t = btree_set<int>::load(data.begin(), data.end()); });
}

TEST(BTreeSet, Parallel) {
    //        const int N = 600000000;
    //        const int N = 100000;
    //        const int N = 10000;
    const int N = 1000;
    //        const int N = 300;
    //        const int N = 100;

    // get a unordered list of test data
    using entry_t = int;
    std::vector<entry_t> list;
    btree_set<entry_t> filter;

    for (int i = 0; i < N; i++) {
        list.push_back(i);
    }

    // the number of times duplicates show up in the input set
    for (int dup = 1; dup < 4; dup++) {
        // now duplicate this list
        std::vector<entry_t> full;
        for (int i = 0; i < dup; i++) {
            for (const auto& cur : list) {
                full.push_back(cur);
            }
        }

        // shuffle data
        std::random_device rd;
        std::mt19937 generator(rd());

        std::shuffle(full.begin(), full.end(), generator);

        // now insert all those values into a new set - in parallel
        btree_set<entry_t> res;
#ifdef _OPENMP
#pragma omp parallel for  //  schedule(static,1)
#endif
        for (int idx = 0; idx < static_cast<int>(full.size()); ++idx) {
            res.insert(full[idx]);
        }

        EXPECT_TRUE(res.check());

        // check resulting values
        EXPECT_EQ(N, res.size());

        std::set<entry_t> should(full.begin(), full.end());
        std::set<entry_t> is(res.begin(), res.end());

        for (const auto& cur : should) {
            EXPECT_TRUE(res.contains(cur)) << "Missing: " << cur << "\n";
        }

        for (const auto& cur : res) {
            EXPECT_TRUE(should.find(cur) != should.end()) << "Additional: " << cur << "\n"
                                                          << "Contained: " << res.contains(cur) << "\n";
        }

        std::vector<entry_t> extra;
        for (const auto& cur : is) {
            if (should.find(cur) == should.end()) extra.push_back(cur);
        }
        EXPECT_TRUE(extra.empty()) << "Extra elments: " << extra << "\n";

        std::vector<entry_t> missing;
        for (const auto& cur : should) {
            if (is.find(cur) == is.end()) missing.push_back(cur);
        }
        EXPECT_TRUE(missing.empty()) << "Missing elments: " << missing << "\n";
        //                << "All Elements: " << should << "\n";

        EXPECT_EQ(N, should.size());
        EXPECT_EQ(N, is.size());
        EXPECT_EQ(should, is);
    }
}

#ifdef _OPENMP

TEST(BTreeSet, ParallelScaling) {
    using test_set = btree_set<int>;
    using op_context_type = test_set::operation_hints;

    //        const int N = 60000000;     // real benchmark
    //        const int N = 1000000;     // to not run to long for unit testing
    //        const int N = 100000;     // to not run to long for unit testing
    const int N = 1000;  // to not run to long for unit testing

    // create some random data
    std::vector<int> data;
    for (int i = 0; i < N; i++) {
        data.push_back(i);
    }
    std::vector<int> data2 = data;
    std::random_device rd;
    std::mt19937 generator(rd());

    std::shuffle(data.begin(), data.end(), generator);
    std::shuffle(data2.begin(), data2.end(), generator);

    for (int i = 1; i <= 8; i++) {
        test_set t;

        omp_set_num_threads(i);

        double start = omp_get_wtime();

#pragma omp parallel
        {
            op_context_type ctxt;

#pragma omp for
            for (int i = 0; i < N; i++) {
                t.insert(data[i], ctxt);
                t.insert(data2[i], ctxt);
            }
        }

        double end = omp_get_wtime();

        std::cout << "Number of threads: " << i << "[" << (end - start) << "ms]\n";

        //          t.printTree();

        EXPECT_EQ(N, t.size());
        int count = 0;
        int last = -1;
        for (int i : t) {
            EXPECT_EQ(last + 1, i);
            last = i;
            count++;
        }
        EXPECT_EQ(N - 1, last);

        EXPECT_EQ(N, count);
    }
}

#endif
}  // namespace souffle::test
