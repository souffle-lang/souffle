/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NilConstant.h
 *
 * Defines Nil constant class
 *
 ***********************************************************************/

#pragma once

#include "ast/Constant.h"
#include "parser/SrcLocation.h"
#include <string>
#include <utility>

namespace souffle::ast {

/**
 * @class NilConstant
 * @brief Defines the nil constant
 */
class NilConstant : public Constant {
public:
    NilConstant(SrcLocation loc = {});

    static bool classof(const Node*);

private:
    NilConstant* cloning() const override;
};

}  // namespace souffle::ast
