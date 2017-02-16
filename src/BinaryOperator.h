/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BinaryOperator.h
 *
 * Defines binary operators and relational operators.
 *
 ***********************************************************************/
#pragma once

#include <cassert>
#include <iostream>

namespace souffle {

/**
 * Binary Relational Operators
 */
enum class BinaryRelOp {
    __UNDEFINED__,  // undefined operator
    EQ,             // equivalence of two values
    NE,             // whether two values are different
    LT,             // less-than
    LE,             // less-than-or-equal-to
    GT,             // greater-than
    GE,             // greater-than-or-equal-to
    MATCH,          // matching string
    CONTAINS,       // whether a sub-string is contained in a string
    NOT_MATCH,      // not matching string
    NOT_CONTAINS    // whether a sub-string is not contained in a string
};

inline BinaryRelOp negate(BinaryRelOp op) {
    switch (op) {
        case BinaryRelOp::EQ:
            return BinaryRelOp::NE;
        case BinaryRelOp::NE:
            return BinaryRelOp::EQ;
        case BinaryRelOp::LT:
            return BinaryRelOp::GE;
        case BinaryRelOp::LE:
            return BinaryRelOp::GT;
        case BinaryRelOp::GE:
            return BinaryRelOp::LT;
        case BinaryRelOp::GT:
            return BinaryRelOp::LE;
        case BinaryRelOp::MATCH:
            return BinaryRelOp::NOT_MATCH;
        case BinaryRelOp::NOT_MATCH:
            return BinaryRelOp::MATCH;
        case BinaryRelOp::CONTAINS:
            return BinaryRelOp::NOT_CONTAINS;
        case BinaryRelOp::NOT_CONTAINS:
            return BinaryRelOp::CONTAINS;
        default:
            break;
    }
    assert(false && "Unsupported Operator!");
    return op;
}

/**
 * Returns the corresponding symbol for the given relational operator.
 */
inline std::string getSymbolForBinaryRelOp(BinaryRelOp op) {
    switch (op) {
        case BinaryRelOp::EQ:
            return "=";
        case BinaryRelOp::NE:
            return "!=";
        case BinaryRelOp::LT:
            return "<";
        case BinaryRelOp::LE:
            return "<=";
        case BinaryRelOp::GT:
            return ">";
        case BinaryRelOp::GE:
            return ">=";
        case BinaryRelOp::MATCH:
            return "match";
        case BinaryRelOp::CONTAINS:
            return "contains";
        case BinaryRelOp::NOT_MATCH:
            return "not_match";
        case BinaryRelOp::NOT_CONTAINS:
            return "not_contains";
        default:
            break;
    }
    assert(false && "Unsupported Operator!");
    return "?";
}

/**
 * Returns the corresponding relation operator for the given symbol.
 */
inline BinaryRelOp getBinaryRelOpForSymbol(const std::string& symbol) {
    if (symbol == "=") return BinaryRelOp::EQ;
    if (symbol == "!=") return BinaryRelOp::NE;
    if (symbol == "<") return BinaryRelOp::LT;
    if (symbol == "<=") return BinaryRelOp::LE;
    if (symbol == ">=") return BinaryRelOp::GE;
    if (symbol == ">") return BinaryRelOp::GT;
    if (symbol == "match") return BinaryRelOp::MATCH;
    if (symbol == "contains") return BinaryRelOp::CONTAINS;
    if (symbol == "not_match") return BinaryRelOp::NOT_MATCH;
    if (symbol == "not_contains") return BinaryRelOp::NOT_CONTAINS;
    std::cout << "Unrecognised operator: " << symbol << "\n";
    assert(false && "Unsupported Operator!");
    return BinaryRelOp::__UNDEFINED__;
}

/**
 * Returns whether the given relational operator has numeric operands.
 */
inline bool isNumericBinaryRelOp(const BinaryRelOp op) {
    switch (op) {
        case BinaryRelOp::EQ:
        case BinaryRelOp::NE:
        case BinaryRelOp::LT:
        case BinaryRelOp::LE:
        case BinaryRelOp::GE:
        case BinaryRelOp::GT:
            return true;
        case BinaryRelOp::MATCH:
        case BinaryRelOp::NOT_MATCH:
        case BinaryRelOp::CONTAINS:
        case BinaryRelOp::NOT_CONTAINS:
            return false;
        default:
            break;
    }
    assert(false && "Uncovered case!");
    return false;
}

/**
 * Returns whether the given operator has symbolic operands.
 */
inline bool isSymbolicBinaryRelOp(const BinaryRelOp op) {
    return !isNumericBinaryRelOp(op);
}

/**
 * Binary Operators
 */
enum class BinaryOp {
    __UNDEFINED__,  // undefined operator
    ADD,            // addition
    SUB,            // subtraction
    MUL,            // multiplication
    DIV,            // division
    EXP,            // exponent
    MOD,            // modulus
    BAND,           // bitwise and
    BOR,            // bitwise or
    BXOR,           // bitwise exclusive or
    LAND,           // logical and
    LOR,            // logical or
    CAT,            // string concatenation
};

/**
 * Returns the corresponding symbol for the given relational operator.
 */
inline std::string getSymbolForBinaryOp(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD:
            return "+";
        case BinaryOp::SUB:
            return "-";
        case BinaryOp::MUL:
            return "*";
        case BinaryOp::DIV:
            return "/";
        case BinaryOp::EXP:
            return "^";
        case BinaryOp::MOD:
            return "%";
        case BinaryOp::BAND:
            return "band";
        case BinaryOp::BOR:
            return "bor";
        case BinaryOp::BXOR:
            return "bxor";
        case BinaryOp::LAND:
            return "land";
        case BinaryOp::LOR:
            return "lor";
        case BinaryOp::CAT:
            return "cat";
        default:
            break;
    }
    assert(false && "Unsupported Operator!");
    return "?";
}

/**
 * Returns the corresponding operator for the given symbol.
 */
inline BinaryOp getBinaryOpForSymbol(const std::string& symbol) {
    if (symbol == "+") return BinaryOp::ADD;
    if (symbol == "-") return BinaryOp::SUB;
    if (symbol == "*") return BinaryOp::MUL;
    if (symbol == "/") return BinaryOp::DIV;
    if (symbol == "^") return BinaryOp::EXP;
    if (symbol == "%") return BinaryOp::MOD;
    if (symbol == "band") return BinaryOp::BAND;
    if (symbol == "bor") return BinaryOp::BOR;
    if (symbol == "bxor") return BinaryOp::BXOR;
    if (symbol == "land") return BinaryOp::LAND;
    if (symbol == "lor") return BinaryOp::LOR;
    if (symbol == "cat") return BinaryOp::CAT;
    std::cout << "Unrecognised operator: " << symbol << "\n";
    assert(false && "Unsupported Operator!");
    return BinaryOp::__UNDEFINED__;
}

/**
 * Returns whether the given operator has a numeric return value.
 */
inline bool isNumericBinaryOp(const BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::EXP:
        case BinaryOp::BAND:
        case BinaryOp::BOR:
        case BinaryOp::BXOR:
        case BinaryOp::LAND:
        case BinaryOp::LOR:
        case BinaryOp::MOD:
            return true;
        case BinaryOp::CAT:
            return false;
        default:
            break;
    }
    assert(false && "Uncovered case!");
    return false;
}

/**
* Returns whether the given operator has a numeric return value.
*/
inline bool isSymbolicBinaryOp(const BinaryOp op) {
    return !isNumericBinaryOp(op);
}

/**
 * Returns whether the given operator takes numeric arguments.
 */
inline bool binaryOpAcceptsNumbers(const BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::EXP:
        case BinaryOp::BAND:
        case BinaryOp::BOR:
        case BinaryOp::BXOR:
        case BinaryOp::LAND:
        case BinaryOp::LOR:
        case BinaryOp::MOD:
            return true;
        case BinaryOp::CAT:
            return false;
        default:
            break;
    }
    assert(false && "Uncovered case!");
    return false;
}

/**
 * Returns whether the given operator takes symbolic arguments.
 */
inline bool binaryOpAcceptsSymbols(const BinaryOp op) {
    return !binaryOpAcceptsNumbers(op);
}

}  // end of namespace souffle
