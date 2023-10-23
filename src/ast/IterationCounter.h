/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IterationCounter.h
 *
 * Defines a counter functor class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class IterationCounter
 * @brief iteration counter for recursive strata
 */
class IterationCounter : public Argument {
public:
    using Argument::Argument;

protected:
    void print(std::ostream& os) const override;

private:
    IterationCounter* cloning() const override;
};

}  // namespace souffle::ast
