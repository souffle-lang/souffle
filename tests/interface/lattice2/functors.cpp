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

enum Sign { Bottom = 0, Negative, Positive, Top, Zero };

souffle::RamDomain lub([[maybe_unused]] souffle::SymbolTable* symbolTable,
        [[maybe_unused]] souffle::RecordTable* recordTable, souffle::RamDomain arg1,
        souffle::RamDomain arg2) {
    if (arg1 == Bottom) return arg2;
    if (arg2 == Bottom) return arg1;
    if (arg1 == arg2) return arg1;
    return Top;
}

souffle::RamDomain glb([[maybe_unused]] souffle::SymbolTable* symbolTable,
        [[maybe_unused]] souffle::RecordTable* recordTable, souffle::RamDomain arg1,
        souffle::RamDomain arg2) {
    if (arg1 == Bottom) return Bottom;
    if (arg2 == Bottom) return Bottom;
    if (arg1 == Top) return arg2;
    if (arg2 == Top) return arg1;
    if (arg1 == arg2) return arg1;
    if (arg1 == arg2) return arg1;
    return Bottom;
}

}  // end of extern "C"
