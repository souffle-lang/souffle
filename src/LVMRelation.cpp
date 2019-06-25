/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVMRelation.cpp
 *
 * Implement LVM Relations
 *
 ***********************************************************************/

#include "LVMRelation.h"
#include "BTree.h"
#include "Brie.h"
#include "Util.h"
#include <utility>

namespace souffle {

LVMRelation::LVMRelation(std::size_t arity, const std::string& name,
        const std::vector<std::string>& attributeTypes, const MinIndexSelection& orderSet,
        IndexFactory factory)
        : relName(name), arity(arity), attributeTypes(attributeTypes) {
    for (auto order : orderSet.getAllOrders()) {
        // Expand the order to a total order
        std::set<int> set;
        for (const auto& i : order) {
            set.insert(i);
        }
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

void LVMRelation::removeIndex(const size_t& indexPos) {
    // All but one index can be removed, default full index can't be removed.
    assert(indexes.size() > 1 || indexPos != 0);
    indexes[indexPos].reset(nullptr);
}

bool LVMRelation::insert(const TupleRef& tuple) {
    if (!main->insert(tuple)) return false;
    for (const auto& cur : indexes) {
        if (cur.get() == main) continue;
        cur->insert(tuple);
    }
    return true;
}

void LVMRelation::insert(const LVMRelation& other) {
    // TODO: cover this in a smarter way
    for (const auto& cur : other.scan()) {
        insert(cur);
    }
}

bool LVMRelation::contains(const TupleRef& tuple) const {
    return main->contains(tuple);
}

Stream LVMRelation::scan() const {
    return main->scan();
}

Stream LVMRelation::range(const size_t& indexPos, const TupleRef& low, const TupleRef& high) const {
    auto& pos = indexes[indexPos];
    return pos->range(low, high);
}

void LVMRelation::swap(LVMRelation& other) {
    indexes.swap(other.indexes);
}

size_t LVMRelation::getLevel() const {
    return this->level;
}

const std::string& LVMRelation::getName() const {
    return this->relName;
}

const std::vector<std::string>& LVMRelation::getAttributeTypeQualifiers() const {
    return this->attributeTypes;
}

size_t LVMRelation::getArity() const {
    return this->arity;
}

size_t LVMRelation::size() const {
    return main->size();
}

bool LVMRelation::empty() const {
    return main->empty();
}

void LVMRelation::purge() {
    for (auto& index : indexes) {
        index->clear();
    }
}

bool LVMRelation::exists(const TupleRef& tuple) const {
    return main->contains(tuple);
}

void LVMRelation::extend(const LVMRelation& rel) {}

LVMEqRelation::LVMEqRelation(size_t arity, const std::string& name,
        const std::vector<std::string>& attributeTypes, const MinIndexSelection& orderSet)
        : LVMRelation(arity, name, attributeTypes, orderSet) {}

bool LVMEqRelation::insert(const TupleRef& tuple) {
    // TODO: (pnappa) an eqrel check here is all that appears to be needed for implicit additions
    // TODO: future optimisation would require this as a member datatype
    // brave soul required to pass this quest
    // // specialisation for eqrel defs
    // std::unique_ptr<binaryrelation> eqreltuples;
    // in addition, it requires insert functions to insert into that, and functions
    // which allow reading of stored values must be changed to accommodate.
    // e.g. insert =>  eqRelTuples->insert(tuple[0], tuple[1]);

    // for now, we just have a naive & extremely slow version, otherwise known as a O(n^2) insertion
    // ):

    for (auto& newTuple : extend(tuple)) {
        LVMRelation::insert(TupleRef(newTuple, arity));
        delete[] newTuple;
    }
    return true;
}

std::vector<RamDomain*> LVMEqRelation::extend(const TupleRef& tuple) {
    std::vector<RamDomain*> newTuples;

    newTuples.push_back(new RamDomain[2]{tuple[0], tuple[0]});
    newTuples.push_back(new RamDomain[2]{tuple[0], tuple[1]});
    newTuples.push_back(new RamDomain[2]{tuple[1], tuple[0]});
    newTuples.push_back(new RamDomain[2]{tuple[1], tuple[1]});

    std::vector<const RamDomain*> relevantStored;
    for (const TupleRef& vals : this->scan()) {
        if (vals[0] == tuple[0] || vals[0] == tuple[1] || vals[1] == tuple[0] || vals[1] == tuple[1]) {
            relevantStored.push_back(new RamDomain[2]{vals[0], vals[1]});
        }
    }

    for (auto& vals : relevantStored) {
        newTuples.push_back(new RamDomain[2]{vals[0], tuple[0]});
        newTuples.push_back(new RamDomain[2]{vals[0], tuple[1]});
        newTuples.push_back(new RamDomain[2]{vals[1], tuple[0]});
        newTuples.push_back(new RamDomain[2]{vals[1], tuple[1]});
        newTuples.push_back(new RamDomain[2]{tuple[0], vals[0]});
        newTuples.push_back(new RamDomain[2]{tuple[0], vals[1]});
        newTuples.push_back(new RamDomain[2]{tuple[1], vals[0]});
        newTuples.push_back(new RamDomain[2]{tuple[1], vals[1]});
        delete[] vals;
    }
    return newTuples;
}

void LVMEqRelation::extend(const LVMRelation& rel) {
    std::vector<RamDomain*> newTuples;
    // store all values that will be implicitly relevant to the those that we will insert
    for (const auto& tuple : rel.scan()) {
        for (auto& newTuple : extend(tuple)) {
            newTuples.push_back(newTuple);
        }
    }
    for (const auto& newTuple : newTuples) {
        LVMRelation::insert(TupleRef(newTuple, arity));
        delete[] newTuple;
    }
}

LVMIndirectRelation::LVMIndirectRelation(size_t arity, const std::string& name,
        const std::vector<std::string>& attributeTypes, const MinIndexSelection& orderSet)
        : LVMRelation(arity, name, attributeTypes, orderSet, createIndirectIndex) {}

bool LVMIndirectRelation::insert(const TupleRef& tuple) {
    if (main->contains(tuple)) {
        return false;
    }

    int blockIndex = num_tuples / (BLOCK_SIZE / arity);
    int tupleIndex = (num_tuples % (BLOCK_SIZE / arity)) * arity;

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
    num_tuples++;

    return true;
}

bool LVMIndirectRelation::insert(const RamDomain* tuple) {
    return this->insert(TupleRef(tuple, arity));
}

void LVMIndirectRelation::purge() {
    blockList.clear();
    for (auto& cur : indexes) {
        cur->clear();
    }
    num_tuples = 0;
}

}  // namespace souffle
