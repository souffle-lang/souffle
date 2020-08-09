/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "RamTypes.h"
#include "TypeAttribute.h"
#include "utility/MiscUtil.h"
#include <ostream>

namespace souffle {

std::ostream& operator<<(std::ostream& os, TypeAttribute T) {
    switch (T) {
        case TypeAttribute::Symbol: return os << "TypeAttribute::Symbol";
        case TypeAttribute::Signed: return os << "TypeAttribute::Signed";
        case TypeAttribute::Float: return os << "TypeAttribute::Float";
        case TypeAttribute::Unsigned: return os << "TypeAttribute::Unsigned";
        case TypeAttribute::Record: return os << "TypeAttribute::Record";
    }

    fatal("unhandled `TypeAttribute`");
}

bool isNumericType(TypeAttribute ramType) {
    switch (ramType) {
        case TypeAttribute::Signed:
        case TypeAttribute::Unsigned:
        case TypeAttribute::Float: return true;
        case TypeAttribute::Symbol:
        case TypeAttribute::Record: return false;
    }

    fatal("unhandled `TypeAttribute`");
}

}  // namespace souffle
