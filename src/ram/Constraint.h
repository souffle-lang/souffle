/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Constraint.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Constraint
 * @brief Evaluates a binary constraint with respect to two Expressions
 *
 * Condition is true if the constraint (a logical operator
 * such as "<") holds between the two operands
 *
 * The following example checks the equality of
 * the two given tuple elements:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.1 = t1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Constraint : public Condition {
public:
    Constraint(BinaryConstraintOp op, Own<Expression> l, Own<Expression> r)
            : op(op), lhs(std::move(l)), rhs(std::move(r)) {
        assert(lhs != nullptr && "left-hand side of constraint is a null-pointer");
        assert(rhs != nullptr && "right-hand side of constraint is a null-pointer");
    }

    /** @brief Get left-hand side */
    const Expression& getLHS() const {
        return *lhs;
    }

    /** @brief Get right-hand side */
    const Expression& getRHS() const {
        return *rhs;
    }

    /** @brief Get operator symbol */
    BinaryConstraintOp getOperator() const {
        return op;
    }

    Constraint* cloning() const override {
        return new Constraint(op, clone(lhs), clone(rhs));
    }

    void apply(const NodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

protected:
    void print(std::ostream& os) const override {
        os << "(" << *lhs << " ";
        os << toBinaryConstraintSymbol(op);
        os << " " << *rhs << ")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(CONSTRAINT " << toBinaryConstraintSymbol(op) << " ";
        os << *lhs << " ";
        os << " " << *rhs << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Constraint>(node);
        return op == other.op && equal_ptr(lhs, other.lhs) && equal_ptr(rhs, other.rhs);
    }

    NodeVec getChildren() const override {
        return {lhs.get(), rhs.get()};
    }

    /** Operator */
    const BinaryConstraintOp op;

    /** Left-hand side of constraint*/
    Own<Expression> lhs;

    /** Right-hand side of constraint */
    Own<Expression> rhs;
};

}  // namespace souffle::ram
