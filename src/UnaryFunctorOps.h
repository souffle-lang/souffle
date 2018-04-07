/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnaryOperator.h
 *
 * Defines unary operators and relational operators.
 *
 ***********************************************************************/
#pragma once

#include <cassert>

namespace souffle {

/**
 * Unary Operators for functors and constraints
 */
enum class UnaryOp {
    __UNDEFINED__,
    STOI,    // string to integer conversion
    ORD,     // ordinal number of a string
    STRLEN,  // length of a string
    NEG,     // numeric negation
    BNOT,    // bitwise negation
    LNOT,    // logical negation
    SIN,     // mathematical sin
    COS,     // mathematical cos
    TAN,     // mathematical tan
    ASIN,    // mathematical asin
    ACOS,    // mathematical acos
    ATAN,    // mathematical atan
    SINH,    // mathematical sinh
    COSH,    // mathematical cosh
    TANH,    // mathematical tanh
    ASINH,   // mathematical asinh
    ACOSH,   // mathematical acosh
    ATANH,   // mathematical atanh
    LOG,     // mathematical natural logarithm
    EXP      // mathematical natural exponent
};

/**
 * Returns the corresponding symbol for the given relational operator.
 */
inline std::string getSymbolForUnaryOp(UnaryOp op) {
    switch (op) {
        case UnaryOp::STOI:
            return "stoi";
        case UnaryOp::ORD:
            return "ord";
        case UnaryOp::STRLEN:
            return "strlen";
        case UnaryOp::NEG:
            return "-";
        case UnaryOp::BNOT:
            return "bnot";
        case UnaryOp::LNOT:
            return "lnot";
        case UnaryOp::SIN:
            return "sin";
        case UnaryOp::COS:
            return "cos";
        case UnaryOp::TAN:
            return "tan";
        case UnaryOp::ASIN:
            return "asin";
        case UnaryOp::ACOS:
            return "acos";
        case UnaryOp::ATAN:
            return "atan";
        case UnaryOp::SINH:
            return "sinh";
        case UnaryOp::COSH:
            return "cosh";
        case UnaryOp::TANH:
            return "tanh";
        case UnaryOp::ASINH:
            return "asinh";
        case UnaryOp::ACOSH:
            return "acosh";
        case UnaryOp::ATANH:
            return "atanh";
        case UnaryOp::LOG:
            return "log";
        case UnaryOp::EXP:
            return "exp";
        default:
            break;
    }
    assert(false && "Unsupported Operator!");
    return "?";
}

/**
 * Returns the corresponding operator for the given symbol.
 */
inline UnaryOp getUnaryOpForSymbol(const std::string& symbol) {
    if (symbol == "stoi") return UnaryOp::STOI;
    if (symbol == "ord") return UnaryOp::ORD;
    if (symbol == "strlen") return UnaryOp::STRLEN;
    if (symbol == "-") return UnaryOp::NEG;
    if (symbol == "bnot") return UnaryOp::BNOT;
    if (symbol == "lnot") return UnaryOp::LNOT;
    if (symbol == "sin") return UnaryOp::SIN;
    if (symbol == "cos") return UnaryOp::COS;
    if (symbol == "tan") return UnaryOp::TAN;
    if (symbol == "asin") return UnaryOp::ASIN;
    if (symbol == "acos") return UnaryOp::ACOS;
    if (symbol == "atan") return UnaryOp::ATAN;
    if (symbol == "sinh") return UnaryOp::SINH;
    if (symbol == "cosh") return UnaryOp::COSH;
    if (symbol == "tanh") return UnaryOp::TANH;
    if (symbol == "asinh") return UnaryOp::ASINH;
    if (symbol == "acosh") return UnaryOp::ACOSH;
    if (symbol == "atanh") return UnaryOp::ATANH;
    if (symbol == "tan") return UnaryOp::TAN;
    if (symbol == "exp") return UnaryOp::EXP;
    std::cout << "Unrecognised operator: " << symbol << "\n";
    assert(false && "Unsupported Operator!");
    return UnaryOp::__UNDEFINED__;
}

/**
 * Returns whether the given operator has a numeric return value.
 */
inline bool isNumericUnaryOp(const UnaryOp op) {
    switch (op) {
        case UnaryOp::STOI:
        case UnaryOp::ORD:
        case UnaryOp::STRLEN:
        case UnaryOp::NEG:
        case UnaryOp::BNOT:
        case UnaryOp::LNOT:
        case UnaryOp::SIN:
        case UnaryOp::COS:
        case UnaryOp::TAN:
        case UnaryOp::ASIN:
        case UnaryOp::ACOS:
        case UnaryOp::ATAN:
        case UnaryOp::SINH:
        case UnaryOp::COSH:
        case UnaryOp::TANH:
        case UnaryOp::ASINH:
        case UnaryOp::ACOSH:
        case UnaryOp::ATANH:
        case UnaryOp::LOG:
        case UnaryOp::EXP:
            return true;
        default:
            break;
    }
    assert(false && "Uncovered case!");
    return false;
}

/**
 * Returns whether the given operator has a symbolic return value.
 */
inline bool isSymbolicUnaryOp(const UnaryOp op) {
    return !isNumericUnaryOp(op);
}

/**
 * Returns whether the given operator takes a numeric argument.
 */
inline bool unaryOpAcceptsNumbers(const UnaryOp op) {
    switch (op) {
        case UnaryOp::NEG:
        case UnaryOp::BNOT:
        case UnaryOp::LNOT:
        case UnaryOp::SIN:
        case UnaryOp::COS:
        case UnaryOp::TAN:
        case UnaryOp::ASIN:
        case UnaryOp::ACOS:
        case UnaryOp::ATAN:
        case UnaryOp::SINH:
        case UnaryOp::COSH:
        case UnaryOp::TANH:
        case UnaryOp::ASINH:
        case UnaryOp::ACOSH:
        case UnaryOp::ATANH:
        case UnaryOp::LOG:
        case UnaryOp::EXP:
            return true;
        case UnaryOp::ORD:
        case UnaryOp::STOI:
        case UnaryOp::STRLEN:
            return false;
        default:
            break;
    }
    assert(false && "Unsupported operator encountered!");
    return false;
}

/**
 * Returns whether the given operator takes a symbolic argument.
 */
inline bool unaryOpAcceptsSymbols(const UnaryOp op) {
    return !unaryOpAcceptsNumbers(op);
}

}  // end of namespace souffle
