/* * Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#include "RamInterface.h"

namespace souffle {

void RamRelationInterface::iterator_base::operator++() {
    ++it;
}

tuple& RamRelationInterface::iterator_base::operator*() {
    tup.rewind();

    const RamDomain* num = *it;

    // construct the tuple to return
    for (size_t i = 0; i < ramRelationInterface->getArity(); i++) {
        if (*(ramRelationInterface->getAttrType(i)) == 's') {
            std::string s = ramRelationInterface->getSymbolTable().resolve(num[i]);
            tup << s;
        } else {
            tup << num[i];
        }
    }

    return tup;
}

RamRelationInterface::iterator_base* RamRelationInterface::iterator_base::clone() const {
    return new RamRelationInterface::iterator_base(getId(), ramRelationInterface, it);
}

/**
 * Helper function to convert a tuple to a RamDomain pointer
 */
RamDomain* convertTupleToNums(const tuple& t) {

    std::vector<RamDomain> tuple;

    // for (size_t i = 0; i < getArity(); i++) {
    //     RamDomain n;
    //     if (*(getAttrType(i)) == 's') {
    //         std::string s;
    //         t >> s;
    //         n = getSymbolTable().lookup(s.c_str());
    //     } else {
    //         t >> n;
    //     }
    //     tuple.push_back(n);
    // }

    for (size_t i = 0; i < t.size(); i++) {
        tuple.push_back(t[i]);
    }

    return tuple.data();
}

void RamRelationInterface::insert(const tuple& t) {
    ramRelation.insert(convertTupleToNums(t));
}

bool RamRelationInterface::contains(const tuple& t) const {
    return ramRelation.exists(convertTupleToNums(t));
}

bool RamRelationInterface::iterator_base::equal(const Relation::iterator_base& o) const {
    return false;
}

typename RamRelationInterface::iterator RamRelationInterface::begin() {
    return RamRelationInterface::iterator(new RamRelationInterface::iterator_base(id, this, ramRelation.begin()));
}

typename RamRelationInterface::iterator RamRelationInterface::end() {
    return RamRelationInterface::iterator(new RamRelationInterface::iterator_base(id, this, ramRelation.end()));
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
    return ramRelation.getID().getArgTypeQualifier(idx).c_str();
}

const char* RamRelationInterface::getAttrName(size_t idx) const {
    return ramRelation.getID().getArg(idx).c_str();
}

size_t RamRelationInterface::getArity() const {
    return ramRelation.getArity();
}

SymbolTable& RamRelationInterface::getSymbolTable() const {
    return symTable;
}

void SouffleInterpreterInterface::printAll(std::string s) {
}

void SouffleInterpreterInterface::dumpInputs(std::ostream& ss) {
}

void SouffleInterpreterInterface::dumpOutputs(std::ostream& ss) {
}
} // end of namespace souffle
