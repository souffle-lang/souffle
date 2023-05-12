/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Counter.h
 *
 * Defines a counter functor class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class Counter
 * @brief counter functor (incrementing a value after each invocation)
 */
class Counter : public Argument {
public:
    using Argument::Argument;

    Counter(SrcLocation = {});

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    Counter* cloning() const override;
};

}  // namespace souffle::ast
