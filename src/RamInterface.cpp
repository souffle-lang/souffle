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

typename RamRelationInterface::iterator_base* RamRelationInterface::iterator_base::clone() const {
    return new RamRelationInterface::iterator_base(getId(), ramRelationInterface, it);
}

bool RamRelationInterface::iterator_base::equal(const Relation::iterator_base& o) const {
    return false;
}

std::size_t RamRelationInterface::size() {
    return ramRelation->size();
}

typename RamRelationInterface::iterator RamRelationInterface::begin() {
    return RamRelationInterface::iterator(new RamRelationInterface::iterator_base(id, this, ramRelation->begin()));
}

typename RamRelationInterface::iterator RamRelationInterface::end() {
    return RamRelationInterface::iterator(new RamRelationInterface::iterator_base(id, this, ramRelation->end()));
}

bool RamRelationInterface::isOutput() const {
    return ramRelation->getID().isOutput();
}

bool RamRelationInterface::isInput() const {
    return ramRelation->getID().isInput();
}

std::string RamRelationInterface::getName() const {
    return name;
}

const char* RamRelationInterface::getAttrType(size_t idx) const {
    return ramRelation->getID().getArgTypeQualifier(idx).c_str();
}

const char* RamRelationInterface::getAttrName(size_t idx) const {
    return ramRelation->getID().getArg(idx).c_str();
}

size_t RamRelationInterface::getArity() const {
    return ramRelation->getArity();
}

SymbolTable& RamRelationInterface::getSymbolTable() const {
    return symTable;
}

} // end of namespace souffle
