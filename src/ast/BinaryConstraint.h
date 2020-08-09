/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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
#include "ast/Node.h"
#include "ast/NodeMapper.h"
#include "parser/BinaryConstraintOps.h"
#include "parser/SrcLocation.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class AstBinaryConstraint
 * @brief Binary constraint class
 *
 * Example:
 *      x = y
 *
 * A binary constraint has a constraint operator, a left-hand side
 * expression, and right-hand side expression.
 */
class AstBinaryConstraint : public AstConstraint {
public:
    AstBinaryConstraint(BinaryConstraintOp o, Own<AstArgument> ls, Own<AstArgument> rs, SrcLocation loc = {})
            : AstConstraint(std::move(loc)), operation(o), lhs(std::move(ls)), rhs(std::move(rs)) {}

    /** Return left-hand side argument */
    AstArgument* getLHS() const {
        return lhs.get();
    }

    /** Return right-hand side argument */
    AstArgument* getRHS() const {
        return rhs.get();
    }

    /** Return binary operator */
    BinaryConstraintOp getOperator() const {
        return operation;
    }

    /** Set binary operator */
    void setOperator(BinaryConstraintOp op) {
        operation = op;
    }

    AstBinaryConstraint* clone() const override {
        return new AstBinaryConstraint(operation, souffle::clone(lhs), souffle::clone(rhs), getSrcLoc());
    }

    void apply(const AstNodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

    std::vector<const AstNode*> getChildNodes() const override {
        return {lhs.get(), rhs.get()};
    }

protected:
    void print(std::ostream& os) const override {
        os << *lhs << " " << operation << " " << *rhs;
    }

    bool equal(const AstNode& node) const override {
        assert(nullptr != dynamic_cast<const AstBinaryConstraint*>(&node));
        const auto& other = static_cast<const AstBinaryConstraint&>(node);
        return operation == other.operation && equal_ptr(lhs, other.lhs) && equal_ptr(rhs, other.rhs);
    }

    /** Constraint operator */
    BinaryConstraintOp operation;

    /** Left-hand side argument of binary constraint */
    Own<AstArgument> lhs;

    /** Right-hand side argument of binary constraint */
    Own<AstArgument> rhs;
};

}  // end of namespace souffle
