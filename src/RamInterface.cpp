/* * Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#include "RamInterface.h"

namespace souffle {

void RamRelationInterface::iterator_base::operator++() {
    ramRelationInterface->ramRelation++;
}

tuple& RamRelationInterface::iterator_base::operator*() {
    tuple t(ramRelationInterface);

    // get elements of tuple
    RamRelation* origTuple = ramRelationInterface->ramRelation;

    for (size_t i = 0; i < ramRelationInterface->getArity(); i++) {
        RamDomain num;
        *origTuple >> num;

        if (*(ramRelationInterface->getAttrType(i)) == 's') {
            std::string s = ramRelationInterface->getSymbolTable()->resolve(num);
            t << s;
        } else {
            t << num;
        }
    }

    return t;
}

bool RamRelationInterface::isOutput() {
    return ramRelation->getId().isOutput();
}

bool RamRelationInterface::isInput() {
    return ramRelation->getId().isInput();
}

std::string RamRelationInterface::getName() {
    return name;
}

const char* RamRelationInterface::getAttrType(size_t idx) {
    return ramRelation->getID()->getArgTypeQualifier(idx).c_str();
}

const char* RamRelationInterface::getAttrName(size_t idx) {
    return ramRelation->getID()->getArg(idx).c_str();
}

size_t RamRelationInterface::getArity() {
    return ramRelation->getArity();
}

SymbolTable& RamRelationInterface::getSymbolTable() {
    return symTable;
}

} // end of namespace souffle
