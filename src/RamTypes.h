/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTypes.h
 *
 * Defines tuple element type and data type for keys on table columns
 *
 ***********************************************************************/

#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <type_traits>

namespace souffle {

enum class TypeAttribute {
    Symbol,
    Signed,    // Signed number
    Unsigned,  // Unsigned number
    Float,     // Floating point number.
    Record,
};

// Printing of the TypeAttribute Enum.
// To be utilised in synthesizer.
inline std::ostream& operator<<(std::ostream& os, TypeAttribute T) {
    switch (T) {
        case TypeAttribute::Symbol:
            os << "TypeAttribute::Symbol";
            break;
        case TypeAttribute::Signed:
            os << "TypeAttribute::Signed";
            break;
        case TypeAttribute::Float:
            os << "TypeAttribute::Float";
            break;
        case TypeAttribute::Unsigned:
            os << "TypeAttribute::Unsigned";
            break;
        case TypeAttribute::Record:
            os << "TypeAttribute::Record";
    }
    return os;
}

/**
 * Check if type is numeric.
 */
inline bool isNumericType(TypeAttribute ramType) {
    switch (ramType) {
        case TypeAttribute::Signed:
        case TypeAttribute::Unsigned:
        case TypeAttribute::Float:
            return true;
        case TypeAttribute::Symbol:
        case TypeAttribute::Record:
            return false;
    }
    return false;  // silence warning
}

/**
 * Types of elements in a tuple.
 *
 * Default domain has size of 32 bits; may be overridden by user
 * defining RAM_DOMAIN_SIZE.
 */

#ifndef RAM_DOMAIN_SIZE
#define RAM_DOMAIN_SIZE 32
#endif

#if RAM_DOMAIN_SIZE == 64
using RamDomain = int64_t;
using RamSigned = RamDomain;
using RamUnsigned = uint64_t;
// There is not standard fixed size double/float.
using RamFloat = double;
#else
using RamDomain = int32_t;
using RamSigned = RamDomain;
using RamUnsigned = uint32_t;
// There is no standard - fixed size double/float.
using RamFloat = float;
#endif

// Compile time sanity checks
static_assert(std::is_integral<RamSigned>::value && std::is_signed<RamSigned>::value,
        "RamSigned must be represented by a signed type.");
static_assert(std::is_integral<RamUnsigned>::value && !std::is_signed<RamUnsigned>::value,
        "RamUnsigned must be represented by an unsigned type.");
static_assert(std::is_floating_point<RamFloat>::value && sizeof(RamFloat) * 8 == RAM_DOMAIN_SIZE,
        "RamFloat must be represented by a floating point and have the same size as other types.");

template <typename T>
constexpr bool isRamType = (std::is_same<T, RamDomain>::value || std::is_same<T, RamSigned>::value ||
                            std::is_same<T, RamUnsigned>::value || std::is_same<T, RamFloat>::value);

/**
In C++20 there will be a new way to cast between types by reinterpreting bits (std::bit_cast),
but as of January 2020 it is not yet supported.
**/

/** Cast a type by reinterpreting its bits. Domain is restricted to Ram Types only.
 * Template takes two types (second type is never necessary because it can be deduced from the argument)
 * The following always holds
 * For type T and a : T
 * ramBitCast<T>(ramBitCast<RamDomain>(a)) == a
 **/
template <typename To = RamDomain, typename From>
To ramBitCast(From source) {
    static_assert(isRamType<From> && isRamType<To>, "Bit casting should only be used on Ram Types.");
    static_assert(sizeof(To) == sizeof(From), "Can't bit cast types with different size.");
    To destination;
    memcpy(&destination, &source, sizeof(destination));
    return destination;
}

/** lower and upper boundaries for the ram types **/
constexpr RamSigned MIN_RAM_SIGNED = std::numeric_limits<RamSigned>::min();
constexpr RamSigned MAX_RAM_SIGNED = std::numeric_limits<RamSigned>::max();

constexpr RamUnsigned MIN_RAM_UNSIGNED = std::numeric_limits<RamUnsigned>::min();
constexpr RamUnsigned MAX_RAM_UNSIGNED = std::numeric_limits<RamUnsigned>::max();

constexpr RamFloat MIN_RAM_FLOAT = std::numeric_limits<RamFloat>::min();
constexpr RamFloat MAX_RAM_FLOAT = std::numeric_limits<RamFloat>::max();

/** search signature of a RAM operation; each bit represents an attribute of a relation.
 * A one represents that the attribute has an assigned value; a zero represents that
 * no value exists (i.e. attribute is unbounded) in the search. */
using SearchSignature = uint64_t;

}  // end of namespace souffle
