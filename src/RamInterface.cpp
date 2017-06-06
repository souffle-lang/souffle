/* * Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#include "RamInterface.h"

namespace souffle {

template <uint32_t id>
void RamRelationInterface<id>::iterator_base::operator++() {
    ++it;
}

template <uint32_t id>
tuple& RamRelationInterface<id>::iterator_base::operator*() {
    tup.rewind();

    const RamDomain* num = *it;

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

template <uint32_t id>
typename RamRelationInterface<id>::iterator_base* RamRelationInterface<id>::iterator_base::clone() const {
    return new RamRelationInterface<id>::iterator_base(getId(), ramRelationInterface, it);
}

template <uint32_t id>
bool RamRelationInterface<id>::iterator_base::equal(const Relation::iterator_base& o) const {
    return false;
}

template <uint32_t id>
std::size_t RamRelationInterface<id>::size() {
    return ramRelation->size();
}

template <uint32_t id>
typename RamRelationInterface<id>::iterator RamRelationInterface<id>::begin() {
    return RamRelationInterface<id>::iterator(new RamRelationInterface<id>::iterator_base(id, this, ramRelation->begin()));
}

template <uint32_t id>
typename RamRelationInterface<id>::iterator RamRelationInterface<id>::end() {
    return RamRelationInterface<id>::iterator(new RamRelationInterface<id>::iterator_base(id, this, ramRelation->end()));
}

template <uint32_t id>
bool RamRelationInterface<id>::isOutput() const {
    return ramRelation->getID().isOutput();
}

template <uint32_t id>
bool RamRelationInterface<id>::isInput() const {
    return ramRelation->getID().isInput();
}

template <uint32_t id>
std::string RamRelationInterface<id>::getName() const {
    return name;
}

template <uint32_t id>
const char* RamRelationInterface<id>::getAttrType(size_t idx) const {
    return ramRelation->getID().getArgTypeQualifier(idx).c_str();
}

template <uint32_t id>
const char* RamRelationInterface<id>::getAttrName(size_t idx) const {
    return ramRelation->getID().getArg(idx).c_str();
}

template <uint32_t id>
size_t RamRelationInterface<id>::getArity() const {
    return ramRelation->getArity();
}

template <uint32_t id>
SymbolTable& RamRelationInterface<id>::getSymbolTable() const {
    return symTable;
}

} // end of namespace souffle
