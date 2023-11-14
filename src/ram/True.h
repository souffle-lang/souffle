/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file True.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class True
 * @brief True value condition
 *
 * Output is "true"
 */
class True : public Condition {
public:
    True* cloning() const override {
        return new True();
    }

protected:
    void print(std::ostream& os) const override {
        os << "TRUE";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "TRUE";
    }
};

}  // namespace souffle::ram
