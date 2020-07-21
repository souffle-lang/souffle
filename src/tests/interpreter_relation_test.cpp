/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file interpreter_relation_test.h
 *
 * Tests InterpreterRelInterface
 *
 ***********************************************************************/

#include "tests/test.h"

#include "InterpreterProgInterface.h"
#include "InterpreterRelation.h"
#include "SouffleInterface.h"
#include "SymbolTable.h"
#include "ram/analysis/RamIndexAnalysis.h"
#include <iosfwd>
#include <string>
#include <utility>

namespace souffle::test {

TEST(Relation0, Construction) {
    // create a nullary relation
    SymbolTable symbolTable;
    MinIndexSelection order{};
    order.insertDefaultTotalIndex(0);
    InterpreterRelation rel(0, 0, "test", {}, order);
    InterpreterRelInterface relInt(rel, symbolTable, "test", {}, {}, 0);

    // add some values
    EXPECT_EQ(0, rel.size());
    relInt.insert(tuple(&relInt, {}));
    EXPECT_EQ(1, rel.size());
    relInt.insert(tuple(&relInt, {}));
    EXPECT_EQ(1, rel.size());
}

TEST(Relation1, Construction) {
    // create a single attribute relation
    SymbolTable symbolTable;
    MinIndexSelection order{};
    order.insertDefaultTotalIndex(1);
    InterpreterRelation rel(1, 0, "test", {"i"}, order);
    InterpreterRelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    tuple d1(&relInt, {1});
    // add some values
    EXPECT_EQ(0, rel.size());
    relInt.insert(d1);
    EXPECT_EQ(1, rel.size());
    relInt.insert(tuple(&relInt, {2}));
    EXPECT_EQ(2, rel.size());
    relInt.insert(tuple(&relInt, {3}));
    EXPECT_EQ(3, rel.size());
    relInt.insert(tuple(&relInt, {4}));
    EXPECT_EQ(4, rel.size());
}

TEST(Basic, Iteration) {
    // create a relation
    SymbolTable symbolTable;
    MinIndexSelection order{};
    order.insertDefaultTotalIndex(1);
    InterpreterRelation rel(1, 0, "test", {"i"}, order);
    InterpreterRelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add some values
    relInt.insert(tuple(&relInt, {1}));
    relInt.insert(tuple(&relInt, {2}));
    relInt.insert(tuple(&relInt, {3}));
    relInt.insert(tuple(&relInt, {4}));

    // Iterate
    Relation::iterator it = relInt.begin();
    std::size_t count = 0;
    while (it != relInt.end()) {
        // Check the 'deref' doesn't crash
        auto value = (*it)[0];
        (void)value;
        ++it;
        ++count;
    }
    EXPECT_EQ(4, count);
}

TEST(Independence, Iteration) {
    // create a table
    SymbolTable symbolTable;
    MinIndexSelection order{};
    order.insertDefaultTotalIndex(1);
    InterpreterRelation rel(1, 0, "test", {"i"}, order);
    InterpreterRelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add a value
    relInt.insert(tuple(&relInt, {1}));

    // Test a iterator returns the correct value
    Relation::iterator it = relInt.begin();
    EXPECT_EQ(1, (*it)[0]);

    // Copy the iterator and modify the copy
    {
        Relation::iterator it2(it);
        EXPECT_EQ(1, (*it2)[0]);
        ++it2;
    }
    EXPECT_EQ(1, (*it)[0]);

    // Test that a new iterator is also valid
    Relation::iterator it3 = relInt.begin();
    EXPECT_EQ(1, (*it3)[0]);
}

TEST(IndependentMoving, Iteration) {
    // create a table
    SymbolTable symbolTable;
    MinIndexSelection order{};
    order.insertDefaultTotalIndex(1);
    InterpreterRelation rel(1, 0, "test", {"i"}, order);
    InterpreterRelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add a value
    relInt.insert(tuple(&relInt, {1}));

    Relation::iterator it = relInt.begin();
    EXPECT_EQ(1, (*it)[0]);

    // Make a new iterator, move it to the first iterator, then let the new iterator go out of scope
    {
        Relation::iterator it2(relInt.begin());
        EXPECT_EQ(1, (*it2)[0]);
        it = std::move(it2);
    }
    EXPECT_EQ(1, (*it)[0]);
}

TEST(IndependentCopying, Iteration) {
    // create a table
    SymbolTable symbolTable;
    MinIndexSelection order{};
    order.insertDefaultTotalIndex(1);
    InterpreterRelation rel(1, 0, "test", {"i"}, order);
    InterpreterRelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add a value
    relInt.insert(tuple(&relInt, {1}));

    Relation::iterator it = relInt.begin();
    EXPECT_EQ(1, (*it)[0]);

    // Make a new iterator, copy it to the first iterator, then let the new iterator go out of scope
    {
        Relation::iterator it2(relInt.begin());
        EXPECT_EQ(1, (*it2)[0]);
        it = it2;
    }
    EXPECT_EQ(1, (*it)[0]);
}

}  // end namespace souffle::test
