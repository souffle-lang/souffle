/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file interpreter_relation_test.cpp
 *
 * Tests RelInterface
 *
 ***********************************************************************/

#include "tests/test.h"

#include "interpreter/ProgInterface.h"
#include "interpreter/Relation.h"
#include "ram/analysis/Index.h"
#include "souffle/SouffleInterface.h"
#include "souffle/SymbolTable.h"
#include <iosfwd>
#include <string>
#include <utility>

namespace souffle::interpreter::test {

using ::souffle::ram::analysis::AttributeConstraint;
using ::souffle::ram::analysis::IndexCluster;
using ::souffle::ram::analysis::LexOrder;
using ::souffle::ram::analysis::OrderCollection;
using ::souffle::ram::analysis::SearchSet;
using ::souffle::ram::analysis::SearchSignature;
using ::souffle::ram::analysis::SignatureOrderMap;

TEST(Relation0, Construction) {
    // create a nullary relation
    SymbolTableImpl symbolTable;

    // create an index selection from no searches to a default index for arity 0
    SignatureOrderMap mapping;
    SearchSet searches;
    LexOrder emptyOrder;
    OrderCollection orders = {emptyOrder};
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<0, interpreter::Btree> rel(0, "test", indexSelection);

    souffle::Tuple<RamDomain, 0> tuple;
    // add some values
    EXPECT_EQ(0, rel.size());
    rel.insert(tuple);
    EXPECT_EQ(1, rel.size());
    rel.insert(tuple);
    EXPECT_EQ(1, rel.size());
}

TEST(Relation0, Iteration) {
    // create a nullary relation
    SymbolTableImpl symbolTable;

    // create an index selection from no searches to a default index for arity 0
    SignatureOrderMap mapping;
    SearchSet searches;
    LexOrder emptyOrder;
    OrderCollection orders = {emptyOrder};
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<0, interpreter::Btree> rel(0, "test", indexSelection);
    RelationWrapper* wrapper = &rel;

    souffle::Tuple<RamDomain, 0> tuple;

    // empty relation
    EXPECT_EQ(wrapper->begin() == wrapper->end(), true);

    // add some values
    rel.insert(tuple);

    EXPECT_EQ(wrapper->begin() == wrapper->end(), false);
}

TEST(Relation1, Construction) {
    // create a single attribute relation
    SymbolTableImpl symbolTable;

    // create an index selection for a relation with arity 1 with only an existence check
    SignatureOrderMap mapping;
    SearchSignature existenceCheck = SearchSignature::getFullSearchSignature(1);
    SearchSet searches = {existenceCheck};
    LexOrder fullOrder = {0};
    OrderCollection orders = {fullOrder};
    mapping.insert({existenceCheck, fullOrder});
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<1, interpreter::Btree> rel(0, "test", indexSelection);
    RelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

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
    SymbolTableImpl symbolTable;

    // create an index selection for a relation with arity 1 with only an existence check
    SignatureOrderMap mapping;
    SearchSignature existenceCheck = SearchSignature::getFullSearchSignature(1);
    SearchSet searches = {existenceCheck};
    LexOrder fullOrder = {0};
    OrderCollection orders = {fullOrder};
    mapping.insert({existenceCheck, fullOrder});
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<1, interpreter::Btree> rel(0, "test", indexSelection);
    RelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add some values
    relInt.insert(tuple(&relInt, {1}));
    relInt.insert(tuple(&relInt, {2}));
    relInt.insert(tuple(&relInt, {3}));
    relInt.insert(tuple(&relInt, {4}));

    // Iterate
    souffle::Relation::iterator it = relInt.begin();
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
    SymbolTableImpl symbolTable;

    // create an index selection for a relation with arity 1 with only an existence check
    SignatureOrderMap mapping;
    SearchSignature existenceCheck = SearchSignature::getFullSearchSignature(1);
    SearchSet searches = {existenceCheck};
    LexOrder fullOrder = {0};
    OrderCollection orders = {fullOrder};
    mapping.insert({existenceCheck, fullOrder});
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<1, interpreter::Btree> rel(0, "test", indexSelection);
    RelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add a value
    relInt.insert(tuple(&relInt, {1}));

    // Test a iterator returns the correct value
    souffle::Relation::iterator it = relInt.begin();
    EXPECT_EQ(1, (*it)[0]);

    // Copy the iterator and modify the copy
    {
        souffle::Relation::iterator it2(it);
        EXPECT_EQ(1, (*it2)[0]);
        ++it2;
    }
    EXPECT_EQ(1, (*it)[0]);

    // Test that a new iterator is also valid
    souffle::Relation::iterator it3 = relInt.begin();
    EXPECT_EQ(1, (*it3)[0]);
}

TEST(IndependentMoving, Iteration) {
    // create a table
    SymbolTableImpl symbolTable;

    // create an index selection for a relation with arity 1 with only an existence check
    SignatureOrderMap mapping;
    SearchSignature existenceCheck = SearchSignature::getFullSearchSignature(1);
    SearchSet searches = {existenceCheck};
    LexOrder fullOrder = {0};
    OrderCollection orders = {fullOrder};
    mapping.insert({existenceCheck, fullOrder});
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<1, interpreter::Btree> rel(0, "test", indexSelection);
    RelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add a value
    relInt.insert(tuple(&relInt, {1}));

    souffle::Relation::iterator it = relInt.begin();
    EXPECT_EQ(1, (*it)[0]);

    // Make a new iterator, move it to the first iterator, then let the new iterator go out of scope
    {
        souffle::Relation::iterator it2(relInt.begin());
        EXPECT_EQ(1, (*it2)[0]);
        it = std::move(it2);
    }
    EXPECT_EQ(1, (*it)[0]);
}

TEST(IndependentCopying, Iteration) {
    // create a table
    SymbolTableImpl symbolTable;

    // create an index selection for a relation with arity 1 with only an existence check
    SignatureOrderMap mapping;
    SearchSignature existenceCheck = SearchSignature::getFullSearchSignature(1);
    SearchSet searches = {existenceCheck};
    LexOrder fullOrder = {0};
    OrderCollection orders = {fullOrder};
    mapping.insert({existenceCheck, fullOrder});
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<1, interpreter::Btree> rel(0, "test", indexSelection);
    RelInterface relInt(rel, symbolTable, "test", {"i"}, {"i"}, 0);

    // add a value
    relInt.insert(tuple(&relInt, {1}));

    souffle::Relation::iterator it = relInt.begin();
    EXPECT_EQ(1, (*it)[0]);

    // Make a new iterator, copy it to the first iterator, then let the new iterator go out of scope
    {
        souffle::Relation::iterator it2(relInt.begin());
        EXPECT_EQ(1, (*it2)[0]);
        it = it2;
    }
    EXPECT_EQ(1, (*it)[0]);
}

TEST(Reordering, Iteration) {
    // create a relation, with a non-default ordering.
    SymbolTableImpl symbolTable;

    // create an index selection for a relation with arity 1 with only an existence check
    SignatureOrderMap mapping;
    SearchSignature existenceCheck = SearchSignature::getFullSearchSignature(3);
    SearchSet searches = {existenceCheck};
    // create an index of order {0, 2, 1}
    LexOrder fullOrder = {0, 2, 1};
    OrderCollection orders = {fullOrder};
    mapping.insert({existenceCheck, fullOrder});
    IndexCluster indexSelection(mapping, searches, orders);

    Relation<3, interpreter::Btree> rel(0, "test", indexSelection);
    souffle::Tuple<RamDomain, 3> tuple{0, 1, 2};
    rel.insert(tuple);

    // Scan should give undecoded tuple.
    {
        const auto& t = *(rel.scan().begin());
        EXPECT_EQ((souffle::Tuple<RamDomain, 3>{0, 2, 1}), t);
    }

    // For-each should give decoded tuple.
    {
        auto t = rel.begin();
        EXPECT_EQ(0, (*t)[0]);
        EXPECT_EQ(1, (*t)[1]);
        EXPECT_EQ(2, (*t)[2]);
    }

    RelInterface relInt(rel, symbolTable, "test", {"i", "i", "i"}, {"i", "i", "i"}, 3);

    // ProgInterface should give decoded tuple.
    {
        const auto it = relInt.begin();
        EXPECT_EQ(0, (*it)[0]);
        EXPECT_EQ(1, (*it)[1]);
        EXPECT_EQ(2, (*it)[2]);
    }
}

}  // namespace souffle::interpreter::test
