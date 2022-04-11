/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file functors.cpp
 *
 * Implementing a sub-list/prefix check for a list
 *
 ***********************************************************************/

#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"
#include <cassert>
#include <stack>

extern "C" {

souffle::RamDomain isSubsequence([[maybe_unused]] souffle::SymbolTable* symbolTable,
        souffle::RecordTable* recordTable, souffle::RamDomain arg1, souffle::RamDomain arg2) {
    assert(symbolTable && "NULL symbol table");
    assert(recordTable && "NULL record table");

    std::stack<souffle::RamDomain> stack1, stack2;

    // unwind first node-sequence onto stack1
    while (arg1 != 0) {
        const souffle::RamDomain* t = recordTable->unpack(arg1, 2);
        stack1.push(t[0]);
        arg1 = t[1];
    }

    // unwind second node-sequence onto stack2
    while (arg2 != 0) {
        const souffle::RamDomain* t = recordTable->unpack(arg2, 2);
        stack2.push(t[0]);
        arg2 = t[1];
    }

    // prefix check
    while (!stack1.empty() && !stack2.empty()) {
        auto x = stack1.top(), y = stack2.top();
        if (x != y) {
            return 0;
        }
        stack1.pop();
        stack2.pop();
    }
    return stack1.empty();
}

}  // end of extern "C"
