/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UndefValue.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class UndefValue
 * @brief An undefined expression
 *
 * Output is ⊥
 */
class UndefValue : public Expression {
public:
    UndefValue* cloning() const override {
        return new UndefValue();
    }

protected:
    void print(std::ostream& os) const override {
        os << "UNDEF";
    }
    void print_sexpr(std::ostream& os) const override {
        os << "UNDEF";
    }
};

}  // namespace souffle::ram
