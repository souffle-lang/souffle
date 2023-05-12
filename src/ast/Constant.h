/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Constant.h
 *
 * Defines an abstract class for constants
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class Constant
 * @brief Abstract constant class
 */
class Constant : public Argument {
public:
    /** Get string representation of Constant */
    const std::string& getConstant() const {
        return constant;
    }

    static bool classof(const Node*);

protected:
    Constant(NodeKind kind, std::string value, SrcLocation loc = {});

    void print(std::ostream& os) const override;

    bool equal(const Node& node) const override;

private:
    /** String representation of constant */
    const std::string constant;
};

}  // namespace souffle::ast
