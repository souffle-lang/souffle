/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All Rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeAttribute.h
 *
 * Defines the type attribute enum
 *
 ***********************************************************************/

#pragma once

#include <iostream>

namespace souffle {

/**
 * @class TypeAttribute
 * @brief Type attribute class
 */
enum class TypeAttribute {
    Symbol,    // Symbol
    Signed,    // Signed number
    Unsigned,  // Unsigned number
    Float,     // Floating point number.
    Record,    // Record
};

// Printing of the TypeAttribute Enum.
// To be utilised in synthesizer.
std::ostream& operator<<(std::ostream& os, TypeAttribute T);

/**
 * Check if type is numeric.
 */
bool isNumericType(TypeAttribute ramType);

}  // end of namespace souffle
