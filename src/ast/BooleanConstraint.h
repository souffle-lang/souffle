/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BooleanConstraint.h
 *
 * Defines the boolean constraint class
 *
 ***********************************************************************/

#pragma once

#include "ast/Constraint.h"
#include "parser/SrcLocation.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class BooleanConstraint
 * @brief Boolean constraint class
 *
 * Example:
 *       true
 *
 * Boolean constraint representing either the 'true' or the 'false' value
 */
class BooleanConstraint : public Constraint {
public:
    BooleanConstraint(bool truthValue, SrcLocation loc = {});

    /** Check whether constraint holds */
    bool isTrue() const {
        return truthValue;
    }

    /** Set truth value */
    void set(bool value) {
        truthValue = value;
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    BooleanConstraint* cloning() const override;

private:
    /** Truth value of Boolean constraint */
    bool truthValue;
};

}  // namespace souffle::ast
