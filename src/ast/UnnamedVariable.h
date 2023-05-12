/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnnamedVariable.h
 *
 * Defines the unnamed variable class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class UnnamedVariable
 * @brief Unnamed variable class
 */
class UnnamedVariable : public Argument {
public:
    using Argument::Argument;

    UnnamedVariable(SrcLocation loc = {});

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    UnnamedVariable* cloning() const override;
};

}  // namespace souffle::ast
