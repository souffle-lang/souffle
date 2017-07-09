/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamInterface.cpp
 *
 * Defines classes that implement the SouffleInterface abstract class
 *
 ***********************************************************************/

#include "RamInterface.h"

namespace souffle {

void RamRelationInterface::iterator_base::operator++() {
    ++it;
}

tuple& RamRelationInterface::iterator_base::operator*() {
    tup.rewind();

    // construct the tuple to return
    for (size_t i = 0; i < ramRelationInterface->getArity(); i++) {
        if (*(ramRelationInterface->getAttrType(i)) == 's') {
            std::string s = ramRelationInterface->getSymbolTable().resolve((*it)[i]);
            tup << s;
        } else {
            tup << (*it)[i];
        }
    }

    tup.rewind();

    return tup;
}

RamRelationInterface::iterator_base* RamRelationInterface::iterator_base::clone() const {
    return new RamRelationInterface::iterator_base(getId(), ramRelationInterface, it);
}

bool RamRelationInterface::iterator_base::equal(const Relation::iterator_base& o) const {
    try {
        auto iter = dynamic_cast<const RamRelationInterface::iterator_base&>(o);
        return ramRelationInterface == iter.ramRelationInterface && it == iter.it;
    } catch (const std::bad_cast& e) {
        return false;
    }
}

/**
 * Helper function to convert a tuple to a RamDomain pointer
 */
RamDomain* convertTupleToNums(const tuple& t) {
    RamDomain* newTuple = new RamDomain[t.size()];

    for (size_t i = 0; i < t.size(); i++) {
        newTuple[i] = t[i];
    }

    return newTuple;
}

void RamRelationInterface::insert(const tuple& t) {
    ramRelation.insert(convertTupleToNums(t));
}

bool RamRelationInterface::contains(const tuple& t) const {
    return ramRelation.exists(convertTupleToNums(t));
}

typename RamRelationInterface::iterator RamRelationInterface::begin() {
    return RamRelationInterface::iterator(
            new RamRelationInterface::iterator_base(id, this, ramRelation.begin()));
}

typename RamRelationInterface::iterator RamRelationInterface::end() {
    return RamRelationInterface::iterator(
            new RamRelationInterface::iterator_base(id, this, ramRelation.end()));
}

std::size_t RamRelationInterface::size() {
    return ramRelation.size();
}

bool RamRelationInterface::isOutput() const {
    return ramRelation.getID().isOutput();
}

bool RamRelationInterface::isInput() const {
    return ramRelation.getID().isInput();
}

std::string RamRelationInterface::getName() const {
    return name;
}

const char* RamRelationInterface::getAttrType(size_t idx) const {
    assert(0 <= idx && idx < getArity() && "exceeded tuple size");
    return types[idx].c_str();
}

const char* RamRelationInterface::getAttrName(size_t idx) const {
    assert(0 <= idx && idx < getArity() && "exceeded tuple size");
    return attrNames[idx].c_str();
}

size_t RamRelationInterface::getArity() const {
    return ramRelation.getArity();
}

SymbolTable& RamRelationInterface::getSymbolTable() const {
    return symTable;
}

}  // end of namespace souffle
