/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Location.h
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ast2ram {

struct Location {
    const std::size_t identifier;
    const std::size_t element;

    Location(std::size_t ident, std::size_t elem) : identifier(ident), element(elem) {}

    Location(const Location& l) = default;

    bool operator==(const Location& loc) const {
        return identifier == loc.identifier && element == loc.element;
    }

    bool operator!=(const Location& loc) const {
        return !(*this == loc);
    }

    bool operator<(const Location& loc) const {
        return identifier < loc.identifier || (identifier == loc.identifier && element < loc.element);
    }

    void print(std::ostream& out) const {
        out << "(" << identifier << "," << element << ")";
    }

    friend std::ostream& operator<<(std::ostream& out, const Location& loc) {
        loc.print(out);
        return out;
    }
};

}  // namespace souffle::ast2ram
