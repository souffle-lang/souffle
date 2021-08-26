/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file lib_common.h
 *
 * Trivial functor wrappers.
 * This test only makes use of FFI b/c it's currently the only available
 * standard pragma-appendable option.
 *
 ***********************************************************************/

#include "souffle/RamTypes.h"

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

#ifdef DEFINE_FOO
FF_int foo() {
    return 42;
}
#endif

#ifdef DEFINE_BAR
FF_int bar() {
    return 1337;
}
#endif
}  // end of extern "C"
