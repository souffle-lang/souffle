/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IntrinsicOperator.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "FunctorOps.h"
#include "ram/AbstractOperator.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/tinyformat.h"
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class IntrinsicOperator
 * @brief Operator that represents an intrinsic (built-in) functor
 */
class IntrinsicOperator : public AbstractOperator {
public:
    template <typename... Args>
    IntrinsicOperator(FunctorOp op, Args... args) : AbstractOperator({std::move(args)...}), operation(op) {}

    IntrinsicOperator(FunctorOp op, VecOwn<Expression> args)
            : AbstractOperator(std::move(args)), operation(op) {}

    /** @brief Get operator symbol */
    FunctorOp getOperator() const {
        return operation;
    }

    IntrinsicOperator* cloning() const override {
        VecOwn<Expression> argsCopy;
        for (auto& arg : arguments) {
            argsCopy.emplace_back(arg->cloning());
        }
        return new IntrinsicOperator(operation, std::move(argsCopy));
    }

protected:
    void print(std::ostream& os) const override {
        if (isInfixFunctorOp(operation)) {
            os << "(" << join(arguments, tfm::format("%s", operation)) << ")";
        } else {
            os << operation << "(" << join(arguments) << ")";
        }
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(INTRINSIC " << operation << " " << join(arguments, " ") << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<IntrinsicOperator>(node);
        return AbstractOperator::equal(node) && operation == other.operation;
    }

    /** Operation symbol */
    const FunctorOp operation;
};

}  // namespace souffle::ram
