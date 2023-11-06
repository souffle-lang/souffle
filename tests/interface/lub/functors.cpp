/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file functors.cpp
 *
 ***********************************************************************/

#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"
#include <cassert>

extern "C" {

souffle::RamDomain ilub([[maybe_unused]] souffle::SymbolTable* symbolTable,
        souffle::RecordTable* recordTable, souffle::RamDomain arg1, souffle::RamDomain arg2) {

    const souffle::RamDomain* t1 = recordTable->unpack(arg1, 2);
    const souffle::RamDomain* t2 = recordTable->unpack(arg2, 2);
    //std::cout << "ilub " << "[" << t1[0] << "," << t1[1] << "] [" << t2[0] << "," << t2[1] << "] = [" << std::min(t1[0], t2[0]) << "," << std::max(t1[1], t2[1]) << "]" << std::endl;
    const souffle::RamDomain res[2] = {std::min(t1[0], t2[0]), std::max(t1[1], t2[1])};
    return recordTable->pack(res, 2);
}

souffle::RamDomain iwiden([[maybe_unused]] souffle::SymbolTable* symbolTable,
        souffle::RecordTable* recordTable, souffle::RamDomain arg1, souffle::RamDomain arg2) {
    const souffle::RamDomain* t1 = recordTable->unpack(arg1, 2);
    const souffle::RamDomain* t2 = recordTable->unpack(arg2, 2);
    const souffle::RamDomain lub[2] = {std::min(t1[0], t2[0]), std::max(t1[1], t2[1])};
    const souffle::RamDomain res[2] = {lub[0] < t1[0] ? -1000 : t1[0], lub[1] > t1[1] ? 1000 : t1[1]};
    return recordTable->pack(res, 2);
}

}  // end of extern "C"
