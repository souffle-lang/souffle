/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Variable.h
 *
 * Define the variable class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class Variable
 * @brief Named variable class
 */
class Variable : public Argument {
public:
    Variable(std::string name, SrcLocation loc = {});

    /** Set variable name */
    void setName(std::string name);

    /** Return variable name */
    const std::string& getName() const {
        return name;
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    Variable* cloning() const override;

private:
    /** Name */
    std::string name;
};

}  // namespace souffle::ast
