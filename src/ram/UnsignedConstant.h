/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnsignedConstant.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/NumericConstant.h"
#include "souffle/RamTypes.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class UnsignedConstant
 * @brief Represents a unsigned constant
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * unsigned(5)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class UnsignedConstant : public NumericConstant {
public:
    explicit UnsignedConstant(RamUnsigned val) : NumericConstant(ramBitCast(val)) {}

    /** @brief Get value of the constant. */
    RamUnsigned getValue() const {
        return ramBitCast<RamUnsigned>(constant);
    }

    /** Create cloning */
    UnsignedConstant* cloning() const override {
        return new UnsignedConstant(getValue());
    }

protected:
    void print(std::ostream& os) const override {
        os << "UNSIGNED(" << getValue() << ")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(UNSIGNED " << getValue() << ")";
    }
};

}  // namespace souffle::ram
