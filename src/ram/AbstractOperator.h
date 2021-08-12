/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AbstractOperator.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AbstractOperator
 * @brief Abstract class for an operator/functor
 */
class AbstractOperator : public Expression {
public:
    explicit AbstractOperator(VecOwn<Expression> args) : arguments(std::move(args)) {
        for (auto const& arg : arguments) {
            assert(arg != nullptr && "argument is null-pointer");
        }
    }

    /** @brief Get argument values */
    std::vector<Expression*> getArguments() const {
        return toPtrVector(arguments);
    }

    void apply(const NodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

protected:
    bool equal(const Node& node) const override {
        const auto& other = asAssert<AbstractOperator>(node);
        return equal_targets(arguments, other.arguments);
    }

    NodeVec getChildNodesImpl() const override {
        return toPtrVector<Node const>(arguments);
    }

    /** Arguments of user defined operator */
    VecOwn<Expression> arguments;
};

}  // namespace souffle::ram
