/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecordInit.h
 *
 * Defines the record initialization class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/Term.h"
#include "parser/SrcLocation.h"
#include <iostream>
#include <string>

namespace souffle::ast {

/**
 * @class RecordInit
 * @brief Defines a record initialization class
 */
class RecordInit : public Term {
public:
    RecordInit(VecOwn<Argument> operands = {}, SrcLocation loc = {});

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    RecordInit* cloning() const override;
};

}  // namespace souffle::ast
