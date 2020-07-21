/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterRelation.cpp
 *
 * Implement Interpreter Relations
 *
 ***********************************************************************/

#include "InterpreterRelation.h"
#include "ram/analysis/RamIndexAnalysis.h"
#include <algorithm>
#include <cassert>
#include <set>
#include <utility>

namespace souffle {

InterpreterRelation::InterpreterRelation(std::size_t arity, std::size_t auxiliaryArity, std::string name,
        std::vector<std::string> attributeTypes, const MinIndexSelection& orderSet, IndexFactory factory)
        : relName(std::move(name)), arity(arity), auxiliaryArity(auxiliaryArity),
          attributeTypes(std::move(attributeTypes)) {
    for (auto order : orderSet.getAllOrders()) {
        // Expand the order to a total order
        AttributeSet set{order.begin(), order.end()};

        for (std::size_t i = 0; i < arity; ++i) {
            if (set.find(i) == set.end()) {
                order.push_back(i);
            }
        }
        indexes.push_back(factory(Order(order)));
    }

    // Use the first index as default main index
    main = indexes[0].get();
}

void InterpreterRelation::removeIndex(const size_t& indexPos) {
    // All but one index can be removed, default full index can't be removed.
    assert(indexes.size() > 1 || indexPos != 0);
    indexes[indexPos].reset(nullptr);
}

IndexViewPtr InterpreterRelation::getView(const size_t& indexPos) const {
    assert(indexPos < indexes.size());
    return indexes[indexPos]->createView();
}

bool InterpreterRelation::insert(const TupleRef& tuple) {
    if (!main->insert(tuple)) {
        return false;
    }
    for (const auto& cur : indexes) {
        if (cur.get() == main) {
            continue;
        }
        cur->insert(tuple);
    }
    return true;
}

void InterpreterRelation::insert(const InterpreterRelation& other) {
    // TODO: cover this in a smarter way
    for (const auto& cur : other.scan()) {
        insert(cur);
    }
}

bool InterpreterRelation::contains(const TupleRef& tuple) const {
    return main->contains(tuple);
}

bool InterpreterRelation::contains(const size_t& indexPos, const TupleRef& low, const TupleRef& high) const {
    return indexes[indexPos]->contains(low, high);
}

Stream InterpreterRelation::scan() const {
    return main->scan();
}

PartitionedStream InterpreterRelation::partitionScan(size_t partitionCount) const {
    return main->partitionScan(partitionCount);
}

Stream InterpreterRelation::range(const size_t& indexPos, const TupleRef& low, const TupleRef& high) const {
    auto& pos = indexes[indexPos];
    return pos->range(low, high);
}

PartitionedStream InterpreterRelation::partitionRange(
        const size_t& indexPos, const TupleRef& low, const TupleRef& high, size_t partitionCount) const {
    auto& pos = indexes[indexPos];
    return pos->partitionRange(low, high, partitionCount);
}

void InterpreterRelation::swap(InterpreterRelation& other) {
    indexes.swap(other.indexes);
}

size_t InterpreterRelation::getLevel() const {
    return this->level;
}

const std::string& InterpreterRelation::getName() const {
    return this->relName;
}

const std::vector<std::string>& InterpreterRelation::getAttributeTypes() const {
    return this->attributeTypes;
}

size_t InterpreterRelation::getAuxiliaryArity() const {
    return auxiliaryArity;
}

size_t InterpreterRelation::size() const {
    return main->size();
}

bool InterpreterRelation::empty() const {
    return main->empty();
}

void InterpreterRelation::purge() {
    for (auto& index : indexes) {
        index->clear();
    }
}

bool InterpreterRelation::exists(const TupleRef& tuple) const {
    return main->contains(tuple);
}

void InterpreterRelation::extend(const InterpreterRelation&) {}

InterpreterEqRelation::InterpreterEqRelation(size_t arity, size_t auxiliaryArity, const std::string& name,
        const std::vector<std::string>& attributeTypes, const MinIndexSelection& orderSet)
        : InterpreterRelation(arity, auxiliaryArity, name, attributeTypes, orderSet, createEqrelIndex) {
    // EqivalenceRelation should have only one index.
    assert(this->indexes.size() == 1);
}

void InterpreterEqRelation::extend(const InterpreterRelation& rel) {
    auto otherEqRel = dynamic_cast<const InterpreterEqRelation*>(&rel);
    assert(otherEqRel != nullptr && "A eqRel can only merge with another eqRel");
    this->main->extend(otherEqRel->main);
}

InterpreterIndirectRelation::InterpreterIndirectRelation(size_t arity, size_t auxiliaryArity,
        const std::string& name, const std::vector<std::string>& attributeTypes,
        const MinIndexSelection& orderSet)
        : InterpreterRelation(arity, auxiliaryArity, name, attributeTypes, orderSet, createIndirectIndex) {}

bool InterpreterIndirectRelation::insert(const TupleRef& tuple) {
    if (main->contains(tuple)) {
        return false;
    }

    int blockIndex = numTuples / (BLOCK_SIZE / arity);
    int tupleIndex = (numTuples % (BLOCK_SIZE / arity)) * arity;

    if (tupleIndex == 0) {
        blockList.push_back(std::make_unique<RamDomain[]>(BLOCK_SIZE));
    }

    RamDomain* newTuple = &blockList[blockIndex][tupleIndex];
    for (size_t i = 0; i < arity; ++i) {
        newTuple[i] = tuple[i];
    }

    // update all indexes with new tuple
    for (auto& cur : indexes) {
        cur->insert(TupleRef(newTuple, arity));
    }

    // increment relation size
    numTuples++;

    return true;
}

bool InterpreterIndirectRelation::insert(const RamDomain* tuple) {
    return this->insert(TupleRef(tuple, arity));
}

void InterpreterIndirectRelation::purge() {
    blockList.clear();
    for (auto& cur : indexes) {
        cur->clear();
    }
    numTuples = 0;
}

}  // namespace souffle
