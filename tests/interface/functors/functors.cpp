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
#include <cstdint>
#include <cstring>

extern "C" {

int32_t foo(int32_t n, const char* s) {
    return n + strlen(s);
}

int32_t goo(const char* s, int32_t n) {
    return strlen(s) + n;
}

const char* hoo() {
    return "Hello world!\n";
}
}
