/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FunctorOps.h
 *
 * Defines intrinsic functor operators for AST and RAM
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "TypeAttribute.h"
#include <cstdlib>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace souffle {

enum class FunctorOp {
    /** Unary Functor Operators */
    ORD,     // ordinal number of a string
    STRLEN,  // length of a string
    NEG,     // Signed numeric negation
    FNEG,    // Float numeric negation
    BNOT,    // Signed bitwise negation
    UBNOT,   // Unsigned bitwise negation
    LNOT,    // Signed logical negation
    ULNOT,   // Unsigned logical negation

    F2I,  // float     to signed
    F2S,  // float     to symbol
    F2U,  // float     to unsigned
    I2F,  // signed    to signed
    I2S,  // signed    to symbol     (overload base case)
    I2U,  // signed    to unsigned
    S2F,  // symbol    to float      (overload base case)
    S2I,  // symbol    to signed     (overload base case)
    S2U,  // symbol    to unsigned   (overload base case)
    U2F,  // unsigned  to float
    U2I,  // unsigned  to signed
    U2S,  // unsigned  to symbol

    /** Binary Functor Operators */
    ADD,                 // addition
    SUB,                 // subtraction
    MUL,                 // multiplication
    DIV,                 // division
    EXP,                 // exponent
    MAX,                 // max of two numbers
    MIN,                 // min of two numbers
    MOD,                 // modulus
    BAND,                // bitwise and
    BOR,                 // bitwise or
    BXOR,                // bitwise exclusive or
    BSHIFT_L,            // bitwise shift left
    BSHIFT_R,            // bitwise shift right
    BSHIFT_R_UNSIGNED,   // bitwise shift right (unsigned)
    LAND,                // logical and
    LOR,                 // logical or
    LXOR,                // logical xor
    UADD,                // addition
    USUB,                // subtraction
    UMUL,                // multiplication
    UDIV,                // division
    UEXP,                // exponent
    UMAX,                // max of two numbers
    UMIN,                // min of two numbers
    UMOD,                // modulus
    UBAND,               // bitwise and
    UBOR,                // bitwise or
    UBXOR,               // bitwise exclusive or
    UBSHIFT_L,           // bitwise shift right
    UBSHIFT_R,           // bitwise shift right
    UBSHIFT_R_UNSIGNED,  // bitwise shift right (unsigned)
    ULAND,               // logical and
    ULOR,                // logical or
    ULXOR,               // logical xor
    FADD,                // addition
    FSUB,                // subtraction
    FMUL,                // multiplication
    FDIV,                // division
    FEXP,                // exponent
    FMAX,                // max of two floats
    FMIN,                // min of two floats
    SMAX,                // max of two symbols
    SMIN,                // min of two symbols

    // Produces values within a numeric range. Format is `range(bgn, endExcl, step = 1)`.
    // e.g. `range(0, 5)` produces the sequence `0, 1, 2, 3, 4`.
    //      `range(5, 3.75, -0.5)` produces the sequence `5, 4.5, 4`.
    //      `range(5, x, 0)` produces the sequence `5` iff `x` != 5.
    RANGE,
    URANGE,
    FRANGE,

    CAT,  // string concatenation

    /** Ternary Functor Operators */
    SUBSTR,  // substring
};

std::ostream& operator<<(std::ostream& os, FunctorOp op);

struct IntrinsicFunctor {
    std::string symbol;
    std::vector<TypeAttribute> params;
    TypeAttribute result;
    FunctorOp op;
    bool variadic = false;  // varadic => params.size() == 1
    bool multipleResults = false;
};

using IntrinsicFunctors = std::vector<std::reference_wrapper<const IntrinsicFunctor>>;
IntrinsicFunctors functorBuiltIn(FunctorOp);
IntrinsicFunctors functorBuiltIn(std::string_view symbol);
IntrinsicFunctors functorBuiltIn(std::string_view symbol, const std::vector<TypeAttribute>& params);

// Checks whether a functor operation can have a given argument count.
bool isValidFunctorOpArity(std::string_view symbol, size_t arity);

/**
 * Indicate whether a functor is overloaded.
 * At the moment, the signed versions are treated as representatives (because parser always returns a signed
 * version).
 */
bool isOverloadedFunctor(std::string_view symbol);

// Prefix negation operator is a special case. There are no other unary symbolic
// operators. Internally we name it `negate`, but when pretty printing we want
// to special case this and emit `-`.
constexpr char FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME[] = "negate";

/**
 * Determines whether a functor should be written using infix notation (e.g. `a + b + c`)
 * or prefix notation (e.g. `+(a,b,c)`)
 *
 * Generally follow Haskell convention: functions w/ symbolic names are infix, otherwise prefix.
 * NOTE:  The surface syntax occasionally uses alpha infix operators
 *        For backwards compatibility we translate these into symbolic ops.
 */
bool isInfixFunctorOp(std::string_view symbol);
bool isInfixFunctorOp(FunctorOp op);

/**
 * Given a type of an an attribute it returns the appropriate min/max functor operation
 */

FunctorOp getMinOp(const std::string& type);
FunctorOp getMaxOp(const std::string& type);
}  // end of namespace souffle
