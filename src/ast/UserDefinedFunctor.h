/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UserDefinedFunctor.h
 *
 * Defines the user-defined functor class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/Functor.h"
#include "parser/SrcLocation.h"
#include "souffle/TypeAttribute.h"
#include <iosfwd>
#include <string>

namespace souffle::ast {

/**
 * @class UserDefinedFunctor
 * @brief User-Defined functor class
 */
class UserDefinedFunctor : public Functor {
public:
    explicit UserDefinedFunctor(std::string name);

    UserDefinedFunctor(std::string name, VecOwn<Argument> args, SrcLocation loc = {});

    /** return the name */
    const std::string& getName() const {
        return name;
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    UserDefinedFunctor* cloning() const override;

private:
    /** Name */
    const std::string name;
};

}  // namespace souffle::ast
