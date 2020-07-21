/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MiscUtil.h
 *
 * @brief Datalog project utilities
 *
 ***********************************************************************/

#pragma once

#include "tinyformat.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <windows.h>

/**
 * Windows headers define these and they interfere with the standard library
 * functions.
 */
#undef min
#undef max

/**
 * On windows, the following gcc builtins are missing.
 *
 * In the case of popcountll, __popcnt64 is the windows equivalent.
 *
 * For ctz and ctzll, BitScanForward and BitScanForward64 are the respective
 * windows equivalents.  However ctz is used in a constexpr context, and we can't
 * use BitScanForward, so we implement it ourselves.
 */
#define __builtin_popcountll __popcnt64

constexpr unsigned long __builtin_ctz(unsigned long value) {
    unsigned long trailing_zeroes = 0;
    while ((value = value >> 1) ^ 1) {
        ++trailing_zeroes;
    }
    return trailing_zeroes;
}

inline unsigned long __builtin_ctzll(unsigned long long value) {
    unsigned long trailing_zero = 0;

    if (_BitScanForward64(&trailing_zero, value)) {
        return trailing_zero;
    } else {
        return 64;
    }
}
#endif

// -------------------------------------------------------------------------------
//                               Timing Utils
// -------------------------------------------------------------------------------

namespace souffle {

// a type def for a time point
using time_point = std::chrono::high_resolution_clock::time_point;
using std::chrono::microseconds;

// a shortcut for taking the current time
inline time_point now() {
    return std::chrono::high_resolution_clock::now();
}

// a shortcut for obtaining the time difference in milliseconds
inline long duration_in_us(const time_point& start, const time_point& end) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

// a shortcut for obtaining the time difference in nanoseconds
inline long duration_in_ns(const time_point& start, const time_point& end) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

// -------------------------------------------------------------------------------
//                             Cloning Utilities
// -------------------------------------------------------------------------------

template <typename A>
std::unique_ptr<A> clone(const A* node) {
    return node ? std::unique_ptr<A>(node->clone()) : nullptr;
}

template <typename A>
std::unique_ptr<A> clone(const std::unique_ptr<A>& node) {
    return node ? std::unique_ptr<A>(node->clone()) : nullptr;
}

template <typename A, typename B>
auto clone(const std::pair<A, B>& p) {
    return std::make_pair(clone(p.first), clone(p.second));
}

// -------------------------------------------------------------------------------
//                               Error Utilities
// -------------------------------------------------------------------------------

template <typename... Args>
[[noreturn]] void fatal(const char* format, const Args&... args) {
    tfm::format(std::cerr, format, args...);
    std::cerr << "\n";
    assert(false && "fatal error; see std err");
    abort();
}

// HACK:  Workaround to suppress spurious reachability warnings.
#define UNREACHABLE_BAD_CASE_ANALYSIS fatal("unhandled switch branch");
}  // namespace souffle
