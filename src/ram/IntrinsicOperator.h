/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
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

#include "parser/FunctorOps.h"
#include "ram/AbstractOperator.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "utility/StreamUtil.h"
#include "utility/tinyformat.h"
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamIntrinsicOperator
 * @brief Operator that represents an intrinsic (built-in) functor
 */
class RamIntrinsicOperator : public RamAbstractOperator {
public:
    template <typename... Args>
    RamIntrinsicOperator(FunctorOp op, Args... args)
            : RamAbstractOperator({std::move(args)...}), operation(op) {}

    RamIntrinsicOperator(FunctorOp op, std::vector<std::unique_ptr<RamExpression>> args)
            : RamAbstractOperator(std::move(args)), operation(op) {}

    /** @brief Get operator symbol */
    FunctorOp getOperator() const {
        return operation;
    }

    RamIntrinsicOperator* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> argsCopy;
        for (auto& arg : arguments) {
            argsCopy.emplace_back(arg->clone());
        }
        return new RamIntrinsicOperator(operation, std::move(argsCopy));
    }

protected:
    void print(std::ostream& os) const override {
        if (isInfixFunctorOp(operation)) {
            os << "(" << join(arguments, tfm::format("%s", operation)) << ")";
        } else {
            os << operation << "(" << join(arguments) << ")";
        }
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIntrinsicOperator&>(node);
        return RamAbstractOperator::equal(node) && operation == other.operation;
    }

    /** Operation symbol */
    const FunctorOp operation;
};

}  // end of namespace souffle
