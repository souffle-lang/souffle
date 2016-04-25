/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All Rights reserved
 * 
 * The Universal Permissive License (UPL), Version 1.0
 * 
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the 
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified 
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 * 
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 * 
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and 
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the 
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 * 
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

namespace souffle {

/**
 * Binary Relational Operators
 */
enum class BinaryRelOp {
    EQ,           // equivalence of two values
    NE,           // whether two values are different
    LT,           // less-than
    LE,           // less-than-or-equal-to
    GT,           // greater-than
    GE,           // greater-than-or-equal-to
    MATCH,        // matching string
    CONTAINS,     // whether a sub-string is contained in a string
    NOT_MATCH,    // not matching string
    NOT_CONTAINS  // whether a sub-string is not contained in a string
};

inline BinaryRelOp negate(BinaryRelOp op) 
{
    switch(op) {
        case BinaryRelOp::EQ : return BinaryRelOp::NE;
        case BinaryRelOp::NE : return BinaryRelOp::EQ;
        case BinaryRelOp::LT : return BinaryRelOp::GE;
        case BinaryRelOp::LE : return BinaryRelOp::GT;
        case BinaryRelOp::GE : return BinaryRelOp::LT;
        case BinaryRelOp::GT : return BinaryRelOp::LE;
        case BinaryRelOp::MATCH     : return BinaryRelOp::NOT_MATCH;
        case BinaryRelOp::NOT_MATCH : return BinaryRelOp::MATCH;
        case BinaryRelOp::CONTAINS     : return BinaryRelOp::NOT_CONTAINS;
        case BinaryRelOp::NOT_CONTAINS : return BinaryRelOp::CONTAINS;
    }
    assert(false && "Unsupported Operator!");
    return op;
}

/**
 * Returns the corresponding symbol for the given relational operator.
 */
inline std::string getSymbolForBinaryRelOp(BinaryRelOp op) 
{
    switch(op) {
        case BinaryRelOp::EQ : return "=";
        case BinaryRelOp::NE : return "!=";
        case BinaryRelOp::LT : return "<";
        case BinaryRelOp::LE : return "<=";
        case BinaryRelOp::GT : return ">";
        case BinaryRelOp::GE : return ">=";
        case BinaryRelOp::MATCH : return "match";
        case BinaryRelOp::CONTAINS : return "contains";
        case BinaryRelOp::NOT_MATCH : return "not_match";
        case BinaryRelOp::NOT_CONTAINS : return "not_contains";
    }
    assert(false && "Unsupported Operator!");
    return "?";
}

/**
 * Returns the corresponding relation operator for the given symbol.
 */
inline BinaryRelOp getBinaryRelOpForSymbol(const std::string &symbol) {
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
    return BinaryRelOp::EQ;
}

/**
 * Returns whether the given relational operator has numeric operands.
 */
inline bool isNumericBinaryRelOp(const BinaryRelOp op) {
    switch(op) {
    case BinaryRelOp::EQ:
    case BinaryRelOp::NE:
    case BinaryRelOp::LT:
    case BinaryRelOp::LE:
    case BinaryRelOp::GE:
    case BinaryRelOp::GT: return true;
    case BinaryRelOp::MATCH:
    case BinaryRelOp::NOT_MATCH:
    case BinaryRelOp::CONTAINS:
    case BinaryRelOp::NOT_CONTAINS: return false;
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
    ADD,  // addition
    SUB,  // subtraction
    MUL,  // multiplication
    DIV,  // division
    EXP,  // exponent
    MOD,  // modulus
    BAND, // bitwise and
    BOR,  // bitwise or 
    BXOR, // bitwise exclusive or 
    LAND, // logical and
    LOR,  // logical or 
    CAT,  // string concatenation
};

/**
 * Returns the corresponding symbol for the given relational operator.
 */
inline std::string getSymbolForBinaryOp(BinaryOp op) {
    switch(op) {
    case BinaryOp::ADD : return "+";
    case BinaryOp::SUB : return "-";
    case BinaryOp::MUL : return "*";
    case BinaryOp::DIV : return "/";
    case BinaryOp::EXP : return "^";
    case BinaryOp::MOD : return "%";
    case BinaryOp::BAND : return "band";
    case BinaryOp::BOR : return "bor";
    case BinaryOp::BXOR : return "bxor";
    case BinaryOp::LAND : return "land";
    case BinaryOp::LOR : return "lor";
    case BinaryOp::CAT : return "cat";
    }
    assert(false && "Unsupported Operator!");
    return "?";
}

/**
 * Returns the corresponding operator for the given symbol.
 */
inline BinaryOp getBinaryOpForSymbol(const std::string &symbol) {
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
    return BinaryOp::ADD;
}

/**
 * Returns whether the given operator has numeric operands.
 */
inline bool isNumericBinaryOp(const BinaryOp op) {
    switch(op) {
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
    case BinaryOp::MOD: return true;
    case BinaryOp::CAT: return false;
    }
    assert(false && "Uncovered case!");
    return false;
}

/**
 * Returns whether the given operator has symbolic operands.
 */
inline bool isSymbolicBinaryOp(const BinaryOp op) {
    return !isNumericBinaryOp(op);
}

} // end of namespace souffle

