/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file matching_test.h
 *
 * Test cases for the computation of optimal indices.
 *
 ***********************************************************************/

#include "../IndexSetAnalysis.h"
#include "../RamRelation.h"
#include "test.h"

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>

using namespace std;
using namespace souffle;

RamRelation rel;
class TestAutoIndex : public IndexSet {
public:
    TestAutoIndex() : IndexSet(rel) {}
    /** returns number of unique matchings */
    int getNumMatchings() {
        return matching.getNumMatchings();
    }
};

typedef set<SearchColumns> Nodes;

TEST(Matching, StaticTest_1) {
    TestAutoIndex order;
    Nodes nodes;

    order.addSearch(31);
    nodes.insert(31);
    order.addSearch(23);
    nodes.insert(23);
    order.addSearch(15);
    nodes.insert(15);
    order.addSearch(7);
    nodes.insert(7);
    order.addSearch(5);
    nodes.insert(5);
    order.addSearch(3);
    nodes.insert(3);
    order.addSearch(1);
    nodes.insert(1);

    order.solve();
    int num = order.getNumMatchings();

    EXPECT_EQ(num, 5);
}

TEST(Matching, StaticTest_2) {
    TestAutoIndex order;
    Nodes nodes;

    order.addSearch(121);
    nodes.insert(121);
    order.addSearch(104);
    nodes.insert(104);
    order.addSearch(53);
    nodes.insert(53);
    order.addSearch(49);
    nodes.insert(49);
    order.addSearch(39);
    nodes.insert(39);
    order.addSearch(33);
    nodes.insert(33);
    order.addSearch(32);
    nodes.insert(32);
    order.addSearch(23);
    nodes.insert(23);
    order.addSearch(11);
    nodes.insert(11);
    order.addSearch(7);
    nodes.insert(7);

    order.solve();
    int num = order.getNumMatchings();

    EXPECT_EQ(num, 5);
}
