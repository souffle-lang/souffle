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
 * Defines string constants
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/RamTypes.h"
#include "souffle/utility/StringUtil.h"
#include <string>

namespace souffle::ram {

/**
 * @class Constant
 * @brief Represents a String Constant
 *
 */
class StringConstant : public Expression {
public:
    StringConstant(std::string constant) : constant(constant) {}

    /** @brief Get constant */
    const std::string& getConstant() const {
        return constant;
    }

    StringConstant* cloning() const override {
        return new StringConstant(constant);
    }

protected:
    void print(std::ostream& os) const override {
        os << "STRING(\"" << stringify(constant) << "\")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(STRING \"" << stringify(constant) << "\")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<StringConstant>(node);
        return constant == other.constant;
    }

    /** Constant value */
    const std::string constant;
};

}  // namespace souffle::ram
