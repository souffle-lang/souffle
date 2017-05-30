/* * Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#pragma once

#include "RamInterface.h"

namespace souffle {

void RamRelationInterface::iterator_base::operator++() {
    ramRelation++;
}

tuple& RamRelationInterface::iterator_base::operator*() {

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

} // end of namespace souffle
