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
    /** @brief Get argument values */
    std::vector<Expression*> getArguments() const {
        return toPtrVector(arguments);
    }

    std::size_t getNumArgs() const {
        return arguments.size();
    }

    void apply(const NodeMapper& map) override {
        for (auto& arg : arguments) {
            arg = map(std::move(arg));
        }
    }

    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind >= NK_AbstractOperator && kind < NK_LastAbstractOperator);
    }

protected:
    explicit AbstractOperator(NodeKind kind, VecOwn<Expression> args)
            : Expression(kind), arguments(std::move(args)) {
        assert(allValidPtrs(arguments));
        assert(kind >= NK_AbstractOperator && kind < NK_LastAbstractOperator);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<AbstractOperator>(node);
        return equal_targets(arguments, other.arguments);
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(arguments);
    }

    /** Arguments of user defined operator */
    VecOwn<Expression> arguments;
};

}  // namespace souffle::ram
