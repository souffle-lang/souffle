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

#include <cassert>
#include <cstdlib>
#include <iostream>

namespace souffle {

enum class FunctorOp {
    /** Unary Functor Operators */
    ORD,       // ordinal number of a string
    STRLEN,    // length of a string
    NEG,       // Signed numeric negation
    FNEG,      // Float numeric negation
    BNOT,      // Signed bitwise negation
    UBNOT,     // Unsigned bitwise negation
    LNOT,      // Signed logical negation
    ULNOT,     // Unsigned logical negation
    TONUMBER,  // convert string to number
    TOSTRING,  // convert number to string
    ITOU,      // convert signed number to unsigned
    UTOI,      // convert unsigned number to signed
    ITOF,      // convert signed number to float
    FTOI,      // convert float number to signed number.
    UTOF,      // convert unsigned number to a float.
    FTOU,      // convert float to unsigned number.

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
    FADD,                // addition
    FSUB,                // subtraction
    FMUL,                // multiplication
    FDIV,                // division
    FEXP,                // exponent
    FMAX,                // max of two floats
    FMIN,                // min of two floats
    SMAX,                // max of two symbols
    SMIN,                // min of two symbols

    CAT,  // string concatenation

    /** Ternary Functor Operators */
    SUBSTR,  // substring
};

/**
 * Checks whether a functor operation can have a given argument count.
 */
inline bool isValidFunctorOpArity(const FunctorOp op, const size_t arity) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::ORD:
        case FunctorOp::STRLEN:
        case FunctorOp::NEG:
        case FunctorOp::FNEG:
        case FunctorOp::BNOT:
        case FunctorOp::UBNOT:
        case FunctorOp::LNOT:
        case FunctorOp::ULNOT:
        case FunctorOp::TONUMBER:
        case FunctorOp::TOSTRING:
        case FunctorOp::ITOU:
        case FunctorOp::UTOI:
        case FunctorOp::ITOF:
        case FunctorOp::FTOI:
        case FunctorOp::UTOF:
        case FunctorOp::FTOU:
            return arity == 1;

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
        case FunctorOp::BSHIFT_L:
        case FunctorOp::BSHIFT_R:
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
        case FunctorOp::UADD:
        case FunctorOp::USUB:
        case FunctorOp::UMUL:
        case FunctorOp::UDIV:
        case FunctorOp::UEXP:
        case FunctorOp::UMOD:
        case FunctorOp::UBAND:
        case FunctorOp::UBOR:
        case FunctorOp::UBXOR:
        case FunctorOp::UBSHIFT_L:
        case FunctorOp::UBSHIFT_R:
        case FunctorOp::UBSHIFT_R_UNSIGNED:
        case FunctorOp::ULAND:
        case FunctorOp::ULOR:
        case FunctorOp::FADD:
        case FunctorOp::FSUB:
        case FunctorOp::FMUL:
        case FunctorOp::FDIV:
        case FunctorOp::FEXP:
            return arity == 2;

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR:
            return arity == 3;

        /** Non-fixed */
        case FunctorOp::MAX:
        case FunctorOp::MIN:
        case FunctorOp::UMAX:
        case FunctorOp::UMIN:
        case FunctorOp::FMAX:
        case FunctorOp::FMIN:
        case FunctorOp::SMAX:
        case FunctorOp::SMIN:
        case FunctorOp::CAT:
            return arity >= 2;
    }

    assert(false && "unsupported operator");
    return false;
}

/**
 * Return a string representation of a functor.
 */
inline std::string getSymbolForFunctorOp(const FunctorOp op) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::ITOF:
            return "itof";
        case FunctorOp::ITOU:
            return "itou";
        case FunctorOp::UTOF:
            return "utof";
        case FunctorOp::UTOI:
            return "utoi";
        case FunctorOp::FTOI:
            return "ftoi";
        case FunctorOp::FTOU:
            return "ftou";
        case FunctorOp::ORD:
            return "ord";
        case FunctorOp::STRLEN:
            return "strlen";
        case FunctorOp::NEG:
        case FunctorOp::FNEG:
            return "-";
        case FunctorOp::BNOT:
        case FunctorOp::UBNOT:
            return "bnot";
        case FunctorOp::LNOT:
        case FunctorOp::ULNOT:
            return "lnot";
        case FunctorOp::TONUMBER:
            return "to_number";
        case FunctorOp::TOSTRING:
            return "to_string";

        /** Binary Functor Operators */
        case FunctorOp::ADD:
        case FunctorOp::FADD:
        case FunctorOp::UADD:
            return "+";
        case FunctorOp::SUB:
        case FunctorOp::USUB:
        case FunctorOp::FSUB:
            return "-";
        case FunctorOp::MUL:
        case FunctorOp::UMUL:
        case FunctorOp::FMUL:
            return "*";
        case FunctorOp::DIV:
        case FunctorOp::UDIV:
        case FunctorOp::FDIV:
            return "/";
        case FunctorOp::EXP:
        case FunctorOp::FEXP:
        case FunctorOp::UEXP:
            return "^";
        case FunctorOp::MOD:
        case FunctorOp::UMOD:
            return "%";
        case FunctorOp::BAND:
        case FunctorOp::UBAND:
            return "band";
        case FunctorOp::BOR:
        case FunctorOp::UBOR:
            return "bor";
        case FunctorOp::BXOR:
        case FunctorOp::UBXOR:
            return "bxor";
        case FunctorOp::BSHIFT_L:
        case FunctorOp::UBSHIFT_L:
            return "bshl";
        case FunctorOp::BSHIFT_R:
        case FunctorOp::UBSHIFT_R:
            return "bshr";
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::UBSHIFT_R_UNSIGNED:
            return "bshru";
        case FunctorOp::LAND:
        case FunctorOp::ULAND:
            return "land";
        case FunctorOp::LOR:
        case FunctorOp::ULOR:
            return "lor";

        /* N-ary Functor Operators */
        case FunctorOp::MAX:
        case FunctorOp::UMAX:
        case FunctorOp::FMAX:
        case FunctorOp::SMAX:
            return "max";
        case FunctorOp::MIN:
        case FunctorOp::UMIN:
        case FunctorOp::FMIN:
        case FunctorOp::SMIN:
            return "min";
        case FunctorOp::CAT:
            return "cat";

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR:
            return "substr";
    }

    assert(false && "unsupported operator");
    exit(EXIT_FAILURE);
    return "?";
}

/**
 * Check a functor's return type (codomain).
 */
inline TypeAttribute functorReturnType(const FunctorOp op) {
    switch (op) {
        case FunctorOp::ORD:
        case FunctorOp::STRLEN:
        case FunctorOp::NEG:
        case FunctorOp::BNOT:
        case FunctorOp::LNOT:
        case FunctorOp::TONUMBER:
        case FunctorOp::ADD:
        case FunctorOp::SUB:
        case FunctorOp::MUL:
        case FunctorOp::DIV:
        case FunctorOp::EXP:
        case FunctorOp::BAND:
        case FunctorOp::BOR:
        case FunctorOp::BXOR:
        case FunctorOp::BSHIFT_L:
        case FunctorOp::BSHIFT_R:
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
        case FunctorOp::MOD:
        case FunctorOp::MAX:
        case FunctorOp::MIN:
        case FunctorOp::FTOI:
        case FunctorOp::UTOI:
            return TypeAttribute::Signed;
        case FunctorOp::UBNOT:
        case FunctorOp::ITOU:
        case FunctorOp::FTOU:
        case FunctorOp::ULNOT:
        case FunctorOp::UADD:
        case FunctorOp::USUB:
        case FunctorOp::UMUL:
        case FunctorOp::UDIV:
        case FunctorOp::UEXP:
        case FunctorOp::UMAX:
        case FunctorOp::UMIN:
        case FunctorOp::UMOD:
        case FunctorOp::UBAND:
        case FunctorOp::UBOR:
        case FunctorOp::UBXOR:
        case FunctorOp::UBSHIFT_L:
        case FunctorOp::UBSHIFT_R:
        case FunctorOp::UBSHIFT_R_UNSIGNED:
        case FunctorOp::ULAND:
        case FunctorOp::ULOR:
            return TypeAttribute::Unsigned;
        case FunctorOp::FMAX:
        case FunctorOp::FMIN:
        case FunctorOp::FNEG:
        case FunctorOp::FADD:
        case FunctorOp::FSUB:
        case FunctorOp::ITOF:
        case FunctorOp::UTOF:
        case FunctorOp::FMUL:
        case FunctorOp::FDIV:
        case FunctorOp::FEXP:
            return TypeAttribute::Float;
        case FunctorOp::SMAX:
        case FunctorOp::SMIN:
        case FunctorOp::TOSTRING:
        case FunctorOp::CAT:
        case FunctorOp::SUBSTR:
            return TypeAttribute::Symbol;
    }
    assert(false && "Bad functor return type query");
    exit(EXIT_FAILURE);
    return TypeAttribute::Record;  // Silence warning.
}

/**
 * Check the type of argument indicated by arg (0-indexed) of a functor op.
 */
inline TypeAttribute functorOpArgType(const size_t arg, const FunctorOp op) {
    switch (op) {
        // Special case
        case FunctorOp::ORD:
            assert(false && "ord is a special function that returns a Ram Representation of the element");
        case FunctorOp::ITOF:
        case FunctorOp::ITOU:
        case FunctorOp::NEG:
        case FunctorOp::BNOT:
        case FunctorOp::LNOT:
        case FunctorOp::TOSTRING:
            assert(arg == 0 && "unary functor out of bound");
            return TypeAttribute::Signed;
        case FunctorOp::FNEG:
        case FunctorOp::FTOI:
        case FunctorOp::FTOU:
            assert(arg == 0 && "unary functor out of bound");
            return TypeAttribute::Float;
        case FunctorOp::STRLEN:
        case FunctorOp::TONUMBER:
            assert(arg == 0 && "unary functor out of bound");
            return TypeAttribute::Symbol;
        case FunctorOp::UBNOT:
        case FunctorOp::ULNOT:
        case FunctorOp::UTOI:
        case FunctorOp::UTOF:
            assert(arg == 0 && "unary functor out of bound");
            return TypeAttribute::Unsigned;
        case FunctorOp::ADD:
        case FunctorOp::SUB:
        case FunctorOp::MUL:
        case FunctorOp::DIV:
        case FunctorOp::EXP:
        case FunctorOp::MOD:
        case FunctorOp::BAND:
        case FunctorOp::BOR:
        case FunctorOp::BXOR:
        case FunctorOp::BSHIFT_L:
        case FunctorOp::BSHIFT_R:
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::LAND:
        case FunctorOp::LOR:
            assert(arg < 2 && "binary functor out of bound");
            return TypeAttribute::Signed;
        case FunctorOp::UADD:
        case FunctorOp::USUB:
        case FunctorOp::UMUL:
        case FunctorOp::UDIV:
        case FunctorOp::UEXP:
        case FunctorOp::UMOD:
        case FunctorOp::UBAND:
        case FunctorOp::UBOR:
        case FunctorOp::UBXOR:
        case FunctorOp::UBSHIFT_L:
        case FunctorOp::UBSHIFT_R:
        case FunctorOp::UBSHIFT_R_UNSIGNED:
        case FunctorOp::ULAND:
        case FunctorOp::ULOR:
            assert(arg < 2 && "binary functor out of bound");
            return TypeAttribute::Unsigned;
        case FunctorOp::FADD:
        case FunctorOp::FSUB:
        case FunctorOp::FMUL:
        case FunctorOp::FDIV:
        case FunctorOp::FEXP:
            assert(arg < 2 && "binary functor out of bound");
            return TypeAttribute::Float;
        case FunctorOp::SUBSTR:
            assert(arg < 3 && "ternary functor out of bound");
            if (arg == 0) {
                return TypeAttribute::Symbol;
            } else {
                return TypeAttribute::Signed;  // In the future: Change to unsigned
            }
        case FunctorOp::MAX:
        case FunctorOp::MIN:
            return TypeAttribute::Signed;
        case FunctorOp::UMAX:
        case FunctorOp::UMIN:
            return TypeAttribute::Unsigned;
        case FunctorOp::FMAX:
        case FunctorOp::FMIN:
            return TypeAttribute::Float;
        case FunctorOp::SMAX:
        case FunctorOp::SMIN:
        case FunctorOp::CAT:
            return TypeAttribute::Symbol;
    }
    assert(false && "unsupported operator");
    exit(EXIT_FAILURE);
    return TypeAttribute::Record;  // silence warning.
}

/**
 * Indicate whether a functor is overloaded.
 * At the moment, the signed versions are treated as representatives (because parser always returns a signed
 * version).
 */
inline bool isOverloadedFunctor(const FunctorOp functor) {
    switch (functor) {
        /* Unary */
        case FunctorOp::NEG:
        case FunctorOp::BNOT:
        case FunctorOp::LNOT:
        /* Binary */
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
            break;
        default:
            break;
    }

    return false;
}

/**
 * Convert an overloaded functor, so that it works with the requested type.
 * Example: toType = Float, functor = PLUS -> FPLUS (version of plus working on floats)
 */
inline FunctorOp convertOverloadedFunctor(const FunctorOp functor, const TypeAttribute toType) {
    switch (functor) {
        case FunctorOp::NEG:
            assert(toType == TypeAttribute::Float && "Invalid functor conversion from NEG");
            return FunctorOp::FNEG;
        case FunctorOp::BNOT:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion from UBNOT");
            return FunctorOp::UBNOT;
        case FunctorOp::LNOT:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion from LNOT");
            return FunctorOp::ULNOT;
        /* Binary */
        case FunctorOp::ADD:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FADD;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::UADD;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::SUB:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FSUB;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::USUB;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::MUL:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FMUL;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::UMUL;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::DIV:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FDIV;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::UDIV;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::EXP:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FEXP;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::UEXP;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::MAX:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FMAX;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::UMAX;
            } else if (toType == TypeAttribute::Symbol) {
                return FunctorOp::SMAX;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::MIN:
            if (toType == TypeAttribute::Float) {
                return FunctorOp::FMIN;
            } else if (toType == TypeAttribute::Unsigned) {
                return FunctorOp::UMIN;
            } else if (toType == TypeAttribute::Symbol) {
                return FunctorOp::SMIN;
            }
            assert(false && "Invalid functor conversion");
            break;
        case FunctorOp::BAND:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::UBAND;
        case FunctorOp::BOR:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::UBOR;
        case FunctorOp::BXOR:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::UBXOR;
        case FunctorOp::BSHIFT_L:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::UBSHIFT_L;
        case FunctorOp::BSHIFT_R:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::UBSHIFT_R;
        case FunctorOp::LAND:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::ULAND;
        case FunctorOp::LOR:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::ULOR;
        case FunctorOp::MOD:
            assert(toType == TypeAttribute::Unsigned && "Invalid functor conversion");
            return FunctorOp::UMOD;
        default:
            assert(false && "Invalid functor");
    }
}

/**
 * Determines whether a functor should be written using infix notation (e.g. `a + b + c`)
 * or prefix notation (e.g. `+(a,b,c)`)
 */
inline bool isInfixFunctorOp(const FunctorOp op) {
    switch (op) {
        case FunctorOp::ADD:
        case FunctorOp::FADD:
        case FunctorOp::UADD:
        case FunctorOp::SUB:
        case FunctorOp::USUB:
        case FunctorOp::FSUB:
        case FunctorOp::MUL:
        case FunctorOp::FMUL:
        case FunctorOp::UMUL:
        case FunctorOp::DIV:
        case FunctorOp::FDIV:
        case FunctorOp::UDIV:
        case FunctorOp::EXP:
        case FunctorOp::FEXP:
        case FunctorOp::UEXP:
        case FunctorOp::BAND:
        case FunctorOp::UBAND:
        case FunctorOp::BOR:
        case FunctorOp::UBOR:
        case FunctorOp::BXOR:
        case FunctorOp::BSHIFT_L:
        case FunctorOp::UBSHIFT_L:
        case FunctorOp::BSHIFT_R:
        case FunctorOp::UBSHIFT_R:
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::UBSHIFT_R_UNSIGNED:
        case FunctorOp::UBXOR:
        case FunctorOp::LAND:
        case FunctorOp::ULAND:
        case FunctorOp::LOR:
        case FunctorOp::ULOR:
        case FunctorOp::MOD:
        case FunctorOp::UMOD:
            return true;
        default:
            return false;
    }
}

}  // end of namespace souffle
