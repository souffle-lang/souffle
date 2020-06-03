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
#include <cmath>
#include <cstdint>
#include <cstring>

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
    return round(x);
}
}
