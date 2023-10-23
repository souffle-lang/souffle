/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ValueChecker.h
 *
 * Declares methods to check if value is valid.
 *
 ***********************************************************************/

#pragma once

namespace souffle::ast::analysis {

template <typename Var>
class ValueChecker {
public:
    using value_type = typename Var::property_space::value_type;
    using is_valid_op_type = typename Var::property_space::is_valid_op_type;

    virtual bool valueIsValid(const value_type& value) {
        is_valid_op_type valid_op;
        return valid_op(value);
    }
};

}  // namespace souffle::ast::analysis
