/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
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

#include "parser/AggregateOp.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "utility/ContainerUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamAbstractAggregate
 * @brief Abstract class for aggregation
 *
 * A particular function (e.g. MIN) is applied given a
 * that a condition holds
 */
class RamAbstractAggregate {
public:
    RamAbstractAggregate(
            AggregateOp fun, std::unique_ptr<RamExpression> expr, std::unique_ptr<RamCondition> cond)
            : function(fun), expression(std::move(expr)), condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
        assert(expression != nullptr && "Expression is a null-pointer");
    }

    virtual ~RamAbstractAggregate() = default;

    /** @brief Get condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "Condition of aggregate is a null-pointer");
        return *condition;
    }

    /** @brief Get aggregation function */
    AggregateOp getFunction() const {
        return function;
    }

    /** @brief Get target expression */
    const RamExpression& getExpression() const {
        assert(expression != nullptr && "Expression of aggregate is a null-pointer");
        return *expression;
    }

    std::vector<const RamNode*> getChildNodes() const {
        return {expression.get(), condition.get()};
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

protected:
    bool equal(const RamNode& node) const {
        const auto& other = dynamic_cast<const RamAbstractAggregate&>(node);
        return function == other.function && equal_ptr(expression, other.expression) &&
               equal_ptr(condition, other.condition);
    }

    /** Aggregation function */
    AggregateOp function;

    /** Aggregation expression */
    std::unique_ptr<RamExpression> expression;

    /** Aggregation tuple condition */
    std::unique_ptr<RamCondition> condition;
};

}  // namespace souffle
