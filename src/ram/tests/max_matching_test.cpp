/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file max_matching_test.cpp
 *
 * Test cases for the computation of maximum matching.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "ram/analysis/Index.h"
#include <set>
#include <string>

namespace souffle::ram::analysis::test {

class TestMaxMatching : public MaxMatching {
public:
    TestMaxMatching() : MaxMatching() {}
};

TEST(Matching, StaticTest_1) {
    MaxMatching match;

    match.addEdge(1, 7);
    match.addEdge(1, 8);
    match.addEdge(3, 7);
    match.addEdge(3, 10);
    match.addEdge(4, 9);
    match.addEdge(5, 9);
    match.addEdge(5, 10);
    match.addEdge(6, 12);

    match.solve();
    std::size_t num = match.getNumMatchings();

    EXPECT_EQ(num, 5);
}

TEST(Matching, StaticTest_2) {
    MaxMatching match;

    match.addEdge(1, 6);
    match.addEdge(1, 7);
    match.addEdge(2, 6);
    match.addEdge(2, 10);
    match.addEdge(3, 8);
    match.addEdge(3, 9);
    match.addEdge(4, 6);
    match.addEdge(4, 10);
    match.addEdge(5, 7);
    match.addEdge(5, 9);

    match.solve();
    std::size_t num = match.getNumMatchings();

    EXPECT_EQ(num, 5);
}

}  // namespace souffle::ram::analysis::test
