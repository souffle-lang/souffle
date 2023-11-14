/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SubroutineArgument.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/utility/MiscUtil.h"
#include <cstdlib>
#include <ostream>

namespace souffle::ram {

/**
 * @class SubroutineArgument
 * @brief Access argument of a subroutine
 *
 * Arguments are number from zero 0 to n-1
 * where n is the number of arguments of the
 * subroutine.
 */
class SubroutineArgument : public Expression {
public:
    SubroutineArgument(std::size_t number) : number(number) {}

    /** @brief Get argument */
    std::size_t getArgument() const {
        return number;
    }

    SubroutineArgument* cloning() const override {
        return new SubroutineArgument(number);
    }

protected:
    void print(std::ostream& os) const override {
        os << "ARGUMENT(" << number << ")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(ARGUMENT " << number << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<SubroutineArgument>(node);
        return number == other.number;
    }

    /** Argument number */
    const std::size_t number;
};

}  // namespace souffle::ram
