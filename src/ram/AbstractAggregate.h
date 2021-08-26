/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AbstractAggregate.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AbstractAggregate
 * @brief Abstract class for aggregation
 *
 * A particular function (e.g. MIN) is applied given a
 * that a condition holds
 */
class AbstractAggregate {
public:
    AbstractAggregate(AggregateOp fun, Own<Expression> expr, Own<Condition> cond)
            : function(fun), expression(std::move(expr)), condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
        assert(expression != nullptr && "Expression is a null-pointer");
    }

    virtual ~AbstractAggregate() = default;

    /** @brief Get condition */
    const Condition& getCondition() const {
        assert(condition != nullptr && "Condition of aggregate is a null-pointer");
        return *condition;
    }

    /** @brief Get aggregation function */
    AggregateOp getFunction() const {
        return function;
    }

    /** @brief Get target expression */
    const Expression& getExpression() const {
        assert(expression != nullptr && "Expression of aggregate is a null-pointer");
        return *expression;
    }

    Node::ConstChildNodes getChildNodes() const {
        return Node::ConstChildNodes(getChildren(), detail::RefCaster());
    }

    Node::ChildNodes getChildNodes() {
        return Node::ChildNodes(getChildren(), detail::ConstCaster());
    }

protected:
    void print(std::ostream& os, int /* tabpos */) const {
        switch (function) {
            case AggregateOp::MIN:
            case AggregateOp::FMIN:
            case AggregateOp::UMIN: os << "min "; break;
            case AggregateOp::MAX:
            case AggregateOp::UMAX:
            case AggregateOp::FMAX: os << "max "; break;
            case AggregateOp::SUM:
            case AggregateOp::FSUM:
            case AggregateOp::USUM: os << "sum "; break;
            case AggregateOp::COUNT: os << "count "; break;
            case AggregateOp::MEAN: os << "mean "; break;
        }
        if (function != AggregateOp::COUNT) {
            os << *expression << " ";
        }
    }

    bool equal(const Node& node) const {
        const auto& other = asAssert<AbstractAggregate, AllowCrossCast>(node);
        return function == other.function && equal_ptr(expression, other.expression) &&
               equal_ptr(condition, other.condition);
    }

    std::vector<const Node*> getChildren() const {
        return {expression.get(), condition.get()};
    }

    /** Aggregation function */
    const AggregateOp function;

    /** Aggregation expression */
    Own<Expression> expression;

    /** Aggregation tuple condition */
    Own<Condition> condition;
};

}  // namespace souffle::ram
