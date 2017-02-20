/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file type_system_test.h
 *
 * Tests souffle's type system operations.
 *
 ***********************************************************************/

#include "GraphUtils.h"
#include "Util.h"
#include "test.h"

namespace souffle {

namespace test {

TEST(Graph, Basic) {
    Graph<int> g;

    EXPECT_FALSE(g.hasVertex(1));
    EXPECT_FALSE(g.hasVertex(2));
    EXPECT_FALSE(g.hasVertex(3));

    EXPECT_FALSE(g.hasEdge(1, 2));
    EXPECT_FALSE(g.hasEdge(1, 3));
    EXPECT_FALSE(g.hasEdge(2, 3));

    EXPECT_FALSE(g.hasPath(1, 1));
    EXPECT_FALSE(g.hasPath(1, 2));
    EXPECT_FALSE(g.hasPath(1, 3));
    EXPECT_FALSE(g.hasPath(2, 1));
    EXPECT_FALSE(g.hasPath(2, 2));
    EXPECT_FALSE(g.hasPath(2, 3));
    EXPECT_FALSE(g.hasPath(3, 1));
    EXPECT_FALSE(g.hasPath(3, 2));
    EXPECT_FALSE(g.hasPath(3, 3));

    g.insertEdge(1, 2);

    EXPECT_TRUE(g.hasVertex(1));
    EXPECT_TRUE(g.hasVertex(2));
    EXPECT_FALSE(g.hasVertex(3));

    EXPECT_TRUE(g.hasEdge(1, 2));
    EXPECT_FALSE(g.hasEdge(1, 3));
    EXPECT_FALSE(g.hasEdge(2, 3));

    EXPECT_FALSE(g.hasPath(1, 1));
    EXPECT_TRUE(g.hasPath(1, 2));
    EXPECT_FALSE(g.hasPath(1, 3));
    EXPECT_FALSE(g.hasPath(2, 1));
    EXPECT_FALSE(g.hasPath(2, 2));
    EXPECT_FALSE(g.hasPath(2, 3));
    EXPECT_FALSE(g.hasPath(3, 1));
    EXPECT_FALSE(g.hasPath(3, 2));
    EXPECT_FALSE(g.hasPath(3, 3));

    g.insertEdge(2, 3);

    EXPECT_TRUE(g.hasVertex(1));
    EXPECT_TRUE(g.hasVertex(2));
    EXPECT_TRUE(g.hasVertex(3));

    EXPECT_TRUE(g.hasEdge(1, 2));
    EXPECT_FALSE(g.hasEdge(1, 3));
    EXPECT_TRUE(g.hasEdge(2, 3));

    EXPECT_FALSE(g.hasPath(1, 1));
    EXPECT_TRUE(g.hasPath(1, 2));
    EXPECT_TRUE(g.hasPath(1, 3));
    EXPECT_FALSE(g.hasPath(2, 1));
    EXPECT_FALSE(g.hasPath(2, 2));
    EXPECT_TRUE(g.hasPath(2, 3));
    EXPECT_FALSE(g.hasPath(3, 1));
    EXPECT_FALSE(g.hasPath(3, 2));
    EXPECT_FALSE(g.hasPath(3, 3));

    g.insertEdge(3, 1);

    EXPECT_TRUE(g.hasVertex(1));
    EXPECT_TRUE(g.hasVertex(2));
    EXPECT_TRUE(g.hasVertex(3));

    EXPECT_TRUE(g.hasEdge(1, 2));
    EXPECT_FALSE(g.hasEdge(1, 3));
    EXPECT_TRUE(g.hasEdge(2, 3));

    EXPECT_TRUE(g.hasPath(1, 1));
    EXPECT_TRUE(g.hasPath(1, 2));
    EXPECT_TRUE(g.hasPath(1, 3));
    EXPECT_TRUE(g.hasPath(2, 1));
    EXPECT_TRUE(g.hasPath(2, 2));
    EXPECT_TRUE(g.hasPath(2, 3));
    EXPECT_TRUE(g.hasPath(3, 1));
    EXPECT_TRUE(g.hasPath(3, 2));
    EXPECT_TRUE(g.hasPath(3, 3));

    EXPECT_EQ("digraph {\n\"1\" -> \"2\";\n\"2\" -> \"3\";\n\"3\" -> \"1\"\n}\n", toString(g));
}

}  // end namespace test
}  // end namespace souffle
