/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file StringConstant.h
 *
 * Defines the string constant class
 *
 ***********************************************************************/

#pragma once

#include "ast/Constant.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class StringConstant
 * @brief String constant class
 */
class StringConstant : public Constant {
public:
    explicit StringConstant(std::string value, SrcLocation loc = {});

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    StringConstant* cloning() const override;
};

}  // namespace souffle::ast
