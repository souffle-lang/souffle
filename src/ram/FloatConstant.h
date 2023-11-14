/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FloatConstant.h
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
 * @class FloatConstant
 * @brief Represents a float constant
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * float(3.3)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class FloatConstant : public NumericConstant {
public:
    explicit FloatConstant(RamFloat val) : NumericConstant(ramBitCast(val)) {}

    /** @brief Get value of the constant. */
    RamFloat getValue() const {
        return ramBitCast<RamFloat>(constant);
    }

    /** Create cloning */
    FloatConstant* cloning() const override {
        return new FloatConstant(getValue());
    }

protected:
    void print(std::ostream& os) const override {
        os << "FLOAT(" << getValue() << ")";
    }
    
    void print_sexpr(std::ostream& os) const override {
        os << "(FLOAT " << getValue() << ")";
    }
};

}  // namespace souffle::ram
