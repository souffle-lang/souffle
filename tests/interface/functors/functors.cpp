/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file functors.cpp
 *
 * Testing the user-defined functor interface
 *
 ***********************************************************************/
#include "souffle/SouffleFunctor.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <charconv>
#include <cmath>
#include <deque>
#include <mutex>
#include <string>

#if RAM_DOMAIN_SIZE == 64
using FF_int = int64_t;
using FF_uint = uint64_t;
using FF_float = double;
#else
using FF_int = int32_t;
using FF_uint = uint32_t;
using FF_float = float;
#endif

extern "C" {

FF_int foo(FF_int n, const char* s) {
    return n + strlen(s);
}

FF_int goo(const char* s, FF_int n) {
    return strlen(s) + n;
}

const char* hoo() {
    return "Hello world!\n";
}

const char* ioo(FF_int n) {
    if (n < 0) {
        return "NEG";
    } else if (n == 0) {
        return "ZERO";
    } else {
        return "POS";
    }
}

FF_int factorial(FF_uint x) {
    if (x == 0) {
        return 1;
    }

    FF_uint accum = 1;

    while (x > 1) {
        accum *= x;
        --x;
    }

    return accum;
}

FF_int rnd(FF_float x) {
    return static_cast<FF_int>(round(x));
}

FF_float incr(FF_float x) {
    return x + 1.0;
}

const char* concat(FF_float f, FF_int i, FF_uint u, const char* s) {
    static std::deque<std::string> internStrings;
    static std::mutex mut;

    std::string str = std::to_string(f) + std::to_string(i) + std::to_string(u) + std::string(s);

    std::lock_guard<std::mutex> guard(mut);
    const auto it = internStrings.insert(internStrings.end(), str);
    return it->c_str();
}

// Stateful Functors
souffle::RamDomain mycat(souffle::SymbolTable* symbolTable,
        [[maybe_unused]] souffle::RecordTable* recordTable, souffle::RamDomain arg1,
        souffle::RamDomain arg2) {
    assert(symbolTable && "NULL symbol table");
    assert(recordTable && "NULL record table");
    const std::string& sarg1 = symbolTable->decode(arg1);
    const std::string& sarg2 = symbolTable->decode(arg2);
    std::string result = sarg1 + sarg2;
    return symbolTable->encode(result);
}

souffle::RamDomain myappend([[maybe_unused]] souffle::SymbolTable* symbolTable,
        souffle::RecordTable* recordTable, souffle::RamDomain arg) {
    assert(symbolTable && "NULL symbol table");
    assert(recordTable && "NULL record table");

    if (arg == 0) {
        // Argument is nil
        souffle::RamDomain myTuple[2] = {0, 0};
        // Return [0, nil]
        return recordTable->pack(myTuple, 2);
    } else {
        // Argument is a list element [x, l] where
        // x is a number and l is another list element
        const souffle::RamDomain* myTuple0 = recordTable->unpack(arg, 2);
        souffle::RamDomain myTuple1[2] = {myTuple0[0] + 1, arg};
        // Return [x+1, [x, l]]
        return recordTable->pack(myTuple1, 2);
    }
}

souffle::RamDomain my_to_number_fun(
        souffle::SymbolTable* symbolTable, souffle::RecordTable* recordTable, souffle::RamDomain arg) {
    assert(symbolTable && "NULL symbol table");
    assert(recordTable && "NULL record table");

    // Argument is a list element [x, l] where
    // x is a number and l is another list element
    const souffle::RamDomain* myTuple = recordTable->unpack(arg, 2);
    // This is ugly and error-prone.  We should provide a higher-level API which
    // understands the internal data representation for ADTs
    switch (myTuple[0]) {
        case 0: return myTuple[1];
        case 1: {
            auto const& strVal = symbolTable->decode(myTuple[1]);
            souffle::RamDomain result = 0;
            std::from_chars(strVal.c_str(), strVal.c_str() + strVal.size(), result);
            return result;
        }
        default: souffle::fatal("Invalid ADT case");
    }
}

souffle::RamDomain my_identity(souffle::SymbolTable*, souffle::RecordTable*, souffle::RamDomain arg) {
    return arg;
}
}  // end of extern "C"
