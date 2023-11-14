/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Variable.h
 *
 * Defines a class for representing variables in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "souffle/RamTypes.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class Variable
 * @brief Represents a variable of the RAM
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * variable("foo")
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Variable : public Expression {
public:
    explicit Variable(const std::string name) : name(name) {}

    /** @brief Get value of the constant. */
    const std::string getName() const {
        return name;
    }

    /** Create cloning */
    Variable* cloning() const override {
        return new Variable(getName());
    }

protected:
    void print(std::ostream& os) const override {
        os << "VARIABLE(" << getName() << ")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(VARIABLE " << getName() << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Variable>(node);
        return name == other.name;
    }

    const std::string name;
};

}  // namespace souffle::ram
