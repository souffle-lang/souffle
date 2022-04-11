/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file disjoint_set_property_test.cpp
 *
 * Property (randomized) tests for DisjointSet
 *
 ***********************************************************************/

#include "tests/test.h"

#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "souffle/datastructure/PiggyList.h"
#include "souffle/datastructure/UnionFind.h"

// How many times to repeat each randomized test, i.e., how many random
// disjoint-set structures to create.
#define RANDOM_TESTS 32
// Loop bounds inside randomized tests, i.e., upper bound on how many operations
// or checks to perform on each disjoint-set structure.
#define RANDOM_TEST_SIZE 32

// For some reason, just using `assert` doesn't work, even if cassert is
// included.
void local_assert(bool cond, std::string msg) {
    if (!cond) {
        std::cerr << "Tests failed.\n";
        std::cerr << msg << "\n";
        exit(99);
    }
}

// The ASSERT_* macros make use of scoping in such a way that they can't be used
// outside a TEST macro block.
#define LOCAL_ASSERT_LE(x, y) local_assert(x <= y, #x " <= " #y)
#define LOCAL_ASSERT_LT(x, y) local_assert(x < y, #x " < " #y)
#define LOCAL_ASSERT_EQ(x, y) local_assert(x == y, #x " == " #y)

namespace souffle {
namespace test {

// Every node's rank should be less than or equal to that of its parent.
static void check_parent_rank(souffle::DisjointSet& ds, souffle::PiggyList<parent_t>& nodes) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        auto& b = ds.get(nodes.get(i));
        LOCAL_ASSERT_LE(DisjointSet::b2r(b), DisjointSet::b2r(ds.get(DisjointSet::b2p(b))));
    }
}

// Every node's rank should be less than the size of the disjoint set.
static void check_max_rank(souffle::DisjointSet& ds, souffle::PiggyList<parent_t>& nodes) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        auto& b = ds.get(nodes.get(i));
        LOCAL_ASSERT_LT(DisjointSet::b2r(b), nodes.size());
    }
}

// `pr2b` undoes `b2r` and `b2p`.
static void check_pr2b_undoes_b2r_b2p(souffle::DisjointSet& ds, souffle::PiggyList<parent_t>& nodes) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        auto& b = ds.get(nodes.get(i));
        LOCAL_ASSERT_EQ(DisjointSet::pr2b(DisjointSet::b2p(b), DisjointSet::b2r(b)), b);
    }
}

// Check invariants
static void check(souffle::DisjointSet& ds, souffle::PiggyList<parent_t>& nodes) {
    LOCAL_ASSERT_EQ(ds.size(), nodes.size());
    check_parent_rank(ds, nodes);
    check_max_rank(ds, nodes);
    check_pr2b_undoes_b2r_b2p(ds, nodes);
}

// Perform some number of randomized API calls to produce a random disjoint set.
static souffle::PiggyList<parent_t> randomize_disjoint_set(
        souffle::DisjointSet& ds, std::size_t min_size, std::size_t max_size) {
    assert(max_size > 0);
    souffle::PiggyList<parent_t> nodes;
    auto random_node = [&nodes] { return nodes.get(rand() % nodes.size()); };
    // Must be non-empty
    nodes.append(DisjointSet::b2p(ds.makeNode()));
    // TODO: When this loop is made parallel, there's a segfault in the
    // `unionNodes` operation, probably because of how random_node is
    // implemented. However, it would be great to have this loop be parallel.
    for (int i = 0; i < static_cast<int>((rand() % max_size) + min_size); ++i) {
        // Insertions should be 2x as likely as other operations so we get
        // reasonably sized sets with more than 1 equivalence class.
        if (rand() % 2) {
            nodes.append(DisjointSet::b2p(ds.makeNode()));
        } else {
            if (rand() % 2) {
                ds.unionNodes(random_node(), random_node());
            } else {
                // Other mutating methods
                switch (rand() % 3) {
                    case 0: {
                        ds.findNode(random_node());
                        break;
                    }
                    case 1: {
                        ds.sameSet(random_node(), random_node());
                        break;
                    }
                    default: {
                        // Non-mutating methods (queries) - no need to perform these
                        // often, they can all be about as likely as one another.
                        if (rand() % 2) {
                            ds.size();
                        } else {
                            ds.get(random_node());
                        }
                        break;
                    }
                }
            }
        }
    }
    while (nodes.size() < min_size) {
        nodes.append(DisjointSet::b2p(ds.makeNode()));
    }
    check(ds, nodes);
    return souffle::PiggyList<parent_t>{nodes};
}

// When creating a maximally-disjoint set, each node has rank 0.
TEST(DjTest, MakeNode) {
    souffle::DisjointSet ds;
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int i = 0; i < RANDOM_TEST_SIZE; ++i) {
        souffle::block_t n = ds.makeNode();
        // rank should be 0
        EXPECT_EQ(DisjointSet::b2r(n), 0);
        // parent should be itself
        EXPECT_EQ(n, ds.get(DisjointSet::b2p(n)));
#ifndef _OPENMP
        EXPECT_EQ(ds.size(), static_cast<std::size_t>(i + 1));
#endif  // ifndef _OPENMP
    }
    EXPECT_EQ(ds.size(), RANDOM_TEST_SIZE);
}

// 'findNode' doesn't alter 'size'
TEST(DjTest, FindSize) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        auto nodes = randomize_disjoint_set(ds, 1, RANDOM_TEST_SIZE);
#ifdef _OPENMP
#pragma omp parallel
        srand(omp_get_thread_num());
#pragma omp for
#endif
        for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
            auto size0 = ds.size();
            ds.findNode(nodes.get(rand() % nodes.size()));
            EXPECT_EQ(size0, ds.size());
        }
    }
}

// 'unionNodes' doesn't alter 'size'
TEST(DjTest, UnionSize) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        auto nodes = randomize_disjoint_set(ds, 1, RANDOM_TEST_SIZE);
#ifdef _OPENMP
#pragma omp parallel
        srand(omp_get_thread_num());
#pragma omp for
#endif
        for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
            auto size0 = ds.size();
            ds.unionNodes(nodes.get(rand() % nodes.size()), nodes.get(rand() % nodes.size()));
            EXPECT_EQ(size0, ds.size());
        }
    }
}

// 'findNode' doesn't alter 'sameSet'
TEST(DjTest, FindSameSet) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        auto nodes = randomize_disjoint_set(ds, 1, RANDOM_TEST_SIZE);
#ifdef _OPENMP
#pragma omp parallel
        srand(omp_get_thread_num());
#pragma omp for
#endif
        for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
            parent_t node1 = nodes.get(rand() % nodes.size());
            parent_t node2 = nodes.get(rand() % nodes.size());
            bool are_same = ds.sameSet(node1, node2);
            ds.findNode(nodes.get(rand() % nodes.size()));
            EXPECT_EQ(are_same, ds.sameSet(node1, node2));
        }
    }
}

// 'findNode' returns the same value when called with the same arguments
TEST(DjTest, FindTwice) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        auto nodes = randomize_disjoint_set(ds, 1, RANDOM_TEST_SIZE);
#ifdef _OPENMP
#pragma omp parallel
        srand(omp_get_thread_num());
#pragma omp for
#endif
        for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
            parent_t node = nodes.get(rand() % nodes.size());
            EXPECT_EQ(ds.findNode(node), ds.findNode(node));
        }
    }
}

// 'sameSet' has the following properties:
//
// (1) it is reflexive,
// (3) it is symmetric, and
// (2) it returns the same value when called with the same arguments.
TEST(DjTest, SameSet) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        auto nodes = randomize_disjoint_set(ds, 1, RANDOM_TEST_SIZE);
#ifdef _OPENMP
#pragma omp parallel
        srand(omp_get_thread_num());
#pragma omp for
#endif
        for (int j = 0; j < RANDOM_TEST_SIZE; ++j) {
            parent_t node1 = nodes.get(rand() % nodes.size());
            parent_t node2 = nodes.get(rand() % nodes.size());
            EXPECT_TRUE(ds.sameSet(node1, node1));
            EXPECT_EQ(ds.sameSet(node1, node2), ds.sameSet(node2, node1));
            EXPECT_EQ(ds.sameSet(node1, node2), ds.sameSet(node1, node2));
        }
    }
}

// When two nodes are unioned, the resulting root is the root of the tree of one
// of the nodes
TEST(DjTest, UnionRoot) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        auto nodes = randomize_disjoint_set(ds, 0, RANDOM_TEST_SIZE);
        for (int i = 0; i < rand() % RANDOM_TEST_SIZE; ++i) {
            souffle::block_t x = ds.get(nodes.get(rand() % nodes.size()));
            souffle::block_t y = ds.get(nodes.get(rand() % nodes.size()));
            souffle::parent_t px = DisjointSet::b2p(x);
            souffle::parent_t py = DisjointSet::b2p(y);
            // Save original roots
            souffle::parent_t rx = ds.findNode(px);
            souffle::parent_t ry = ds.findNode(py);
            // Union nodes
            ds.unionNodes(px, py);
            EXPECT_TRUE(ds.sameSet(px, py));
            // New roots are the same node
            souffle::parent_t rx2 = ds.findNode(px);
            souffle::parent_t ry2 = ds.findNode(py);
            EXPECT_EQ(rx2, ry2);
            // One of the old roots is the new root
            EXPECT_TRUE(rx2 == rx || rx2 == ry);
        }
    }
}

TEST(DjTest, ClearClear) {
    for (int i = 0; i < RANDOM_TESTS; ++i) {
        souffle::DisjointSet ds;
        randomize_disjoint_set(ds, 0, RANDOM_TEST_SIZE);
        ds.clear();
        EXPECT_EQ(0, ds.size());
        randomize_disjoint_set(ds, 0, RANDOM_TEST_SIZE);
        ds.clear();
        ds.clear();
        EXPECT_EQ(0, ds.size());
    }
}

// Other test ideas:
//
// - unionNodes is idempotent
// - findNode is idempotent

}  // namespace test
}  // namespace souffle
