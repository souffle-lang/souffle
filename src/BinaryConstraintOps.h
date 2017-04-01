/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BinaryConstraintOps.h
 *
 * Defines binary constraint operators for AST & RAM
 *
 ***********************************************************************/
#pragma once

#include <cassert>
#include <iostream>

namespace souffle {

/******************************************
 * Helper Functions for Binary Constraints
 ******************************************/

/**
 * Binary Constraint Operators
 */
enum class BinaryConstraintOp {
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

/**
 * Negated Constraint Operator
 * Each opeprator requires a negated operator which is
 * necessary for the expansion of complex rule bodies with disjunction and negation.
 */
inline BinaryConstraintOp negatedConstraintOp(BinaryConstraintOp op) {
    switch (op) {
        case BinaryConstraintOp::EQ:
            return BinaryConstraintOp::NE;
        case BinaryConstraintOp::NE:
            return BinaryConstraintOp::EQ;
        case BinaryConstraintOp::LT:
            return BinaryConstraintOp::GE;
        case BinaryConstraintOp::LE:
            return BinaryConstraintOp::GT;
        case BinaryConstraintOp::GE:
            return BinaryConstraintOp::LT;
        case BinaryConstraintOp::GT:
            return BinaryConstraintOp::LE;
        case BinaryConstraintOp::MATCH:
            return BinaryConstraintOp::NOT_MATCH;
        case BinaryConstraintOp::NOT_MATCH:
            return BinaryConstraintOp::MATCH;
        case BinaryConstraintOp::CONTAINS:
            return BinaryConstraintOp::NOT_CONTAINS;
        case BinaryConstraintOp::NOT_CONTAINS:
            return BinaryConstraintOp::CONTAINS;
        default:
            break;
    }
    assert(false && "Unsupported Operator!");
    return op;
}

/**
 * Converts operator to its symbolic representation
 */
inline std::string toBinaryConstraintSymbol(BinaryConstraintOp op) {
    switch (op) {
        case BinaryConstraintOp::EQ:
            return "=";
        case BinaryConstraintOp::NE:
            return "!=";
        case BinaryConstraintOp::LT:
            return "<";
        case BinaryConstraintOp::LE:
            return "<=";
        case BinaryConstraintOp::GT:
            return ">";
        case BinaryConstraintOp::GE:
            return ">=";
        case BinaryConstraintOp::MATCH:
            return "match";
        case BinaryConstraintOp::CONTAINS:
            return "contains";
        case BinaryConstraintOp::NOT_MATCH:
            return "not_match";
        case BinaryConstraintOp::NOT_CONTAINS:
            return "not_contains";
        default:
            break;
    }
    assert(false && "Unsupported Operator!");
    return "?";
}

/**
 * Converts symbolic representation of an operator to the operator
 */
inline BinaryConstraintOp toBinaryConstraintOp(const std::string& symbol) {
    if (symbol == "=") return BinaryConstraintOp::EQ;
    if (symbol == "!=") return BinaryConstraintOp::NE;
    if (symbol == "<") return BinaryConstraintOp::LT;
    if (symbol == "<=") return BinaryConstraintOp::LE;
    if (symbol == ">=") return BinaryConstraintOp::GE;
    if (symbol == ">") return BinaryConstraintOp::GT;
    if (symbol == "match") return BinaryConstraintOp::MATCH;
    if (symbol == "contains") return BinaryConstraintOp::CONTAINS;
    if (symbol == "not_match") return BinaryConstraintOp::NOT_MATCH;
    if (symbol == "not_contains") return BinaryConstraintOp::NOT_CONTAINS;
    std::cout << "Unrecognised operator: " << symbol << "\n";
    assert(false && "Unsupported Operator!");
    return BinaryConstraintOp::__UNDEFINED__;
}

/**
 * Helper Functions for Binary Functors
 */

/**
 * Determines whether arguments of constraint are numeric
 */
inline bool isNumericBinaryConstraintOp(const BinaryConstraintOp op) {
    switch (op) {
        case BinaryConstraintOp::EQ:
        case BinaryConstraintOp::NE:
        case BinaryConstraintOp::LT:
        case BinaryConstraintOp::LE:
        case BinaryConstraintOp::GE:
        case BinaryConstraintOp::GT:
            return true;

        case BinaryConstraintOp::MATCH:
        case BinaryConstraintOp::NOT_MATCH:
        case BinaryConstraintOp::CONTAINS:
        case BinaryConstraintOp::NOT_CONTAINS:
            return false;

        default:
            break;
    }
    assert(false && "Uncovered case!");
    return false;
}

/**
 * Determines whether arguments of constraint are numeric
 */
inline bool isSymbolicBinaryConstraintOp(const BinaryConstraintOp op) {
    return !isNumericBinaryConstraintOp(op);
}

}  // end of namespace souffle
