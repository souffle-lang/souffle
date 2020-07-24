/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file matching_test.cpp
 *
 * Test cases for the computation of optimal indices.
 *
 ***********************************************************************/

#include "tests/test.h"

#include "ram/analysis/RamIndexAnalysis.h"
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

namespace souffle {

class TestAutoIndex : public MinIndexSelection {
public:
    TestAutoIndex() : MinIndexSelection() {}
    /** returns number of unique matchings */
    int getNumMatchings() {
        return matching.getNumMatchings();
    }
};

using Nodes = std::set<SearchSignature>;

SearchSignature setBits(size_t arity, uint64_t mask) {
    SearchSignature search(arity);
    for (size_t i = 0; i < arity; ++i) {
        if (mask % 2) {
            search.set(i, AttributeConstraint::Equal);
        }
        mask /= 2;
    }
    return search;
}

TEST(Matching, StaticTest_1) {
    TestAutoIndex order;
    Nodes nodes;
    size_t arity = 5;

    uint64_t patterns[] = {1, 3, 5, 7, 15, 23, 31};
    for (auto pattern : patterns) {
        SearchSignature search = setBits(arity, pattern);
        order.addSearch(search);
        nodes.insert(search);
    }

    order.solve();
    int num = order.getNumMatchings();

    EXPECT_EQ(num, 5);
}

TEST(Matching, StaticTest_2) {
    TestAutoIndex order;
    Nodes nodes;

    size_t arity = 7;

    uint64_t patterns[] = {7, 11, 23, 32, 33, 39, 49, 53, 104, 121};
    for (auto pattern : patterns) {
        SearchSignature search = setBits(arity, pattern);
        order.addSearch(search);
        nodes.insert(search);
    }

    order.solve();
    int num = order.getNumMatchings();

    EXPECT_EQ(num, 5);
}

TEST(Matching, TestOver64BitSignature) {
    TestAutoIndex order;
    Nodes nodes;

    size_t arity = 100;
    SearchSignature first(arity);
    first.set(99, AttributeConstraint::Equal);
    first.set(75, AttributeConstraint::Equal);
    first.set(50, AttributeConstraint::Equal);
    first.set(25, AttributeConstraint::Equal);
    first.set(0, AttributeConstraint::Equal);

    SearchSignature second(arity);
    second.set(99, AttributeConstraint::Equal);
    second.set(75, AttributeConstraint::Equal);
    second.set(50, AttributeConstraint::Equal);

    SearchSignature third(arity);
    third.set(99, AttributeConstraint::Equal);
    third.set(75, AttributeConstraint::Equal);

    SearchSignature fourth(arity);
    fourth.set(99, AttributeConstraint::Equal);

    SearchSignature fifth(arity);
    fifth.set(25, AttributeConstraint::Equal);
    fifth.set(0, AttributeConstraint::Equal);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    nodes.insert(fifth);

    order.addSearch(first);
    order.addSearch(second);
    order.addSearch(third);
    order.addSearch(fourth);
    order.addSearch(fifth);

    order.solve();
    int num = order.getNumMatchings();

    EXPECT_EQ(num, 3);
}

}  // namespace souffle
