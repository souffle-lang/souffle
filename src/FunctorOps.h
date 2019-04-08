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

#include <cassert>
#include <iostream>

namespace souffle {

enum class FunctorOp {
    /** Unary Functor Operators */
    ORD,       // ordinal number of a string
    STRLEN,    // length of a string
    NEG,       // numeric negation
    BNOT,      // bitwise negation
    LNOT,      // logical negation
    TONUMBER,  // convert string to number
    TOSTRING,  // convert number to string

    /** Binary Functor Operators */
    ADD,   // addition
    SUB,   // subtraction
    MUL,   // multiplication
    DIV,   // division
    EXP,   // exponent
    MOD,   // modulus
    BAND,  // bitwise and
    BOR,   // bitwise or
    BXOR,  // bitwise exclusive or
    LAND,  // logical and
    LOR,   // logical or
    MAX,   // max of two numbers
    MIN,   // min of two numbers
    CAT,   // string concatenation

    /** Ternary Functor Operators */
    SUBSTR,  // addition

    /** Undefined */
    __UNDEFINED__,  // undefined operator
};

/**
 * Gets expected arity of functor
 */
inline size_t getFunctorOpArity(FunctorOp op) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::ORD:
        case FunctorOp::STRLEN:
        case FunctorOp::NEG:
        case FunctorOp::BNOT:
        case FunctorOp::LNOT:
        case FunctorOp::TONUMBER:
        case FunctorOp::TOSTRING:
            return 1U;

        /** Binary Functor Operators */
        case FunctorOp::ADD:
        case FunctorOp::SUB:
        case FunctorOp::MUL:
        case FunctorOp::DIV:
        case FunctorOp::EXP:
        case FunctorOp::MOD:
        case FunctorOp::BAND:
        case FunctorOp::BOR:
        case FunctorOp::BXOR:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
        case FunctorOp::MAX:
        case FunctorOp::MIN:
            return 2U;

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR:
            return 3U;

        /** Arbitrary length Functor Operators */
        case FunctorOp::CAT:
            // An arbitrary length that should not be needed
            return 255U;

            /** Undefined */
        default:
            break;
    }

    assert(false && "unsupported operator");
    return 0U;
}

inline std::string getSymbolForFunctorOp(FunctorOp op) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::ORD:
            return "ord";
        case FunctorOp::STRLEN:
            return "strlen";
        case FunctorOp::NEG:
            return "-";
        case FunctorOp::BNOT:
            return "bnot";
        case FunctorOp::LNOT:
            return "lnot";
        case FunctorOp::TONUMBER:
            return "to_number";
        case FunctorOp::TOSTRING:
            return "to_string";

        /** Binary Functor Operators */
        case FunctorOp::ADD:
            return "+";
        case FunctorOp::SUB:
            return "-";
        case FunctorOp::MUL:
            return "*";
        case FunctorOp::DIV:
            return "/";
        case FunctorOp::EXP:
            return "^";
        case FunctorOp::MOD:
            return "%";
        case FunctorOp::BAND:
            return "band";
        case FunctorOp::BOR:
            return "bor";
        case FunctorOp::BXOR:
            return "bxor";
        case FunctorOp::LAND:
            return "land";
        case FunctorOp::LOR:
            return "lor";
        case FunctorOp::MAX:
            return "max";
        case FunctorOp::MIN:
            return "min";
        case FunctorOp::CAT:
            return "cat";

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR:
            return "substr";

        /** Undefined */
        default:
            break;
    }

    assert(false && "unsupported operator");
    return "?";
}

/**
 * Determines whether the given operator has a numeric return value
 */
inline bool isNumericFunctorOp(const FunctorOp op) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::ORD:
        case FunctorOp::STRLEN:
        case FunctorOp::NEG:
        case FunctorOp::BNOT:
        case FunctorOp::LNOT:
        case FunctorOp::TONUMBER:
            return true;
        case FunctorOp::TOSTRING:
            return false;

        /** Binary Functor Operators */
        case FunctorOp::ADD:
        case FunctorOp::SUB:
        case FunctorOp::MUL:
        case FunctorOp::DIV:
        case FunctorOp::EXP:
        case FunctorOp::BAND:
        case FunctorOp::BOR:
        case FunctorOp::BXOR:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
        case FunctorOp::MOD:
        case FunctorOp::MAX:
        case FunctorOp::MIN:
            return true;
        case FunctorOp::CAT:
            return false;

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR:
            return false;

        /** Undefined */
        default:
            break;
    }

    assert(false && "unsupported operator");
    return false;
}

/*
 * Determines whether the operator has a symbolic return value.
 */
inline bool isSymbolicFunctorOp(const FunctorOp op) {
    return !isNumericFunctorOp(op);
}

/**
 * Determines whether an argument has a number value
 */
inline bool functorOpAcceptsNumbers(size_t arg, const FunctorOp op) {
    size_t expectedArity = getFunctorOpArity(op);
    assert(op == FunctorOp::CAT || (arg >= 0 && arg < expectedArity && "argument out of range"));

    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::NEG:
        case FunctorOp::BNOT:
        case FunctorOp::LNOT:
        case FunctorOp::TOSTRING:
            return true;
        case FunctorOp::ORD:
        case FunctorOp::STRLEN:
        case FunctorOp::TONUMBER:
            return false;

        /** Binary Functor Operators */
        case FunctorOp::ADD:
        case FunctorOp::SUB:
        case FunctorOp::MUL:
        case FunctorOp::DIV:
        case FunctorOp::EXP:
        case FunctorOp::BAND:
        case FunctorOp::BOR:
        case FunctorOp::BXOR:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
        case FunctorOp::MOD:
        case FunctorOp::MAX:
        case FunctorOp::MIN:
            return true;
        case FunctorOp::CAT:
            return false;

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR:
            return arg == 1 || arg == 2;

        /** Undefined */
        default:
            break;
    }

    assert(false && "unsupported operator");
    return false;
}

/**
 * Determines whether an argument has a symbolic value
 */
inline bool functorOpAcceptsSymbols(size_t arg, const FunctorOp op) {
    return !functorOpAcceptsNumbers(arg, op);
}

/**
 * Determines whether a functor should be written using infix notation (e.g. `a + b + c`)
 * or prefix notation (e.g. `+(a,b,c)`)
 */
inline bool isInfixFunctorOp(const FunctorOp op) {
    switch (op) {
        case FunctorOp::ADD:
        case FunctorOp::SUB:
        case FunctorOp::MUL:
        case FunctorOp::DIV:
        case FunctorOp::EXP:
        case FunctorOp::BAND:
        case FunctorOp::BOR:
        case FunctorOp::BXOR:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
        case FunctorOp::MOD:
            return true;
        default:
            return false;
    }
}

}  // end of namespace souffle
