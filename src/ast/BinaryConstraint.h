/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BinaryConstraint.h
 *
 * Defines the binary constraint class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "ast/Constraint.h"
#include "parser/SrcLocation.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/Types.h"
#include <iosfwd>

namespace souffle::ast {

/**
 * @class BinaryConstraint
 * @brief Binary constraint class
 *
 * Example:
 *      x = y
 *
 * A binary constraint has a constraint operator, a left-hand side
 * expression, and right-hand side expression.
 */
class BinaryConstraint : public Constraint {
public:
    BinaryConstraint(BinaryConstraintOp o, Own<Argument> ls, Own<Argument> rs, SrcLocation loc = {});

    /** Return left-hand side argument */
    Argument* getLHS() const {
        return lhs.get();
    }

    /** Return right-hand side argument */
    Argument* getRHS() const {
        return rhs.get();
    }

    /** Return binary operator */
    BinaryConstraintOp getBaseOperator() const {
        return operation;
    }

    /** Set binary operator */
    void setBaseOperator(BinaryConstraintOp op) {
        operation = op;
    }

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

private:
    bool equal(const Node& node) const override;

    BinaryConstraint* cloning() const override;

private:
    /** Constraint (base) operator */
    BinaryConstraintOp operation;

    /** Left-hand side argument of binary constraint */
    Own<Argument> lhs;

    /** Right-hand side argument of binary constraint */
    Own<Argument> rhs;
};

}  // namespace souffle::ast
