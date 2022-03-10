/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IterationNumber.h
 *
 * Defines a class that represents the iteration number in a recursive stratum
 * in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class IterationNumber
 * @brief Returns the current iteration number within a recursive stratum.
 *
 * For a non-recursive stratum, ITERNUM() always gives 0. For a recursive
 * stratum, the first iteration is 1, and each subsequent iteration adds 1. The
 * semantics should be defined by the Synthesiser or Interpreter.
 */
class IterationNumber : public Expression {
public:
    IterationNumber* cloning() const override {
        return new IterationNumber();
    }

protected:
    void print(std::ostream& os) const override {
        os << "ITERNUM()";
    }
};

}  // namespace souffle::ram
