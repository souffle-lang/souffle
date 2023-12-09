/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NumericConstant.h
 *
 * Defines the numeric constant class
 *
 ***********************************************************************/

#pragma once

#include "ast/Constant.h"
#include "parser/SrcLocation.h"
#include "souffle/RamTypes.h"
#include <optional>
#include <string>

namespace souffle::ast {

/**
 * Numeric Constant
 *
 * The constant can be initialized with type.
 * If this is the case, the typesystem will be forced to use it.
 * Otherwise the type is inferred from context.
 */
class NumericConstant : public Constant {
public:
    enum class Type { Int, Uint, Float };

    NumericConstant(RamSigned value);

    NumericConstant(std::string constant, SrcLocation loc);

    NumericConstant(std::string constant, std::optional<Type> fixedType = std::nullopt, SrcLocation loc = {});

    const std::optional<Type>& getFixedType() const {
        return fixedType;
    }

    static bool classof(const Node*);

private:
    bool equal(const Node& node) const override;

    NumericConstant* cloning() const override;

private:
    std::optional<Type> fixedType;
};

}  // namespace souffle::ast
