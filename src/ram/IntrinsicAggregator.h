/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IntrinsicAggregator.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "Aggregator.h"
#include "souffle/utility/DynamicCasting.h"

namespace souffle::ram {

/**
 * @class IntrinsicAggregate
 * @brief Class for intrinsic aggregate functions
 *
 */
class IntrinsicAggregator : public Aggregator {
public:
    IntrinsicAggregator(AggregateOp fun) : function(fun) {}
    virtual ~IntrinsicAggregator() = default;

    /** @brief Get aggregation function */
    AggregateOp getFunction() const {
        return function;
    }

    IntrinsicAggregator* cloning() const override {
        return new IntrinsicAggregator(function);
    }

    bool equal(const Aggregator& agg) const override {
        const auto& other = asAssert<IntrinsicAggregator>(agg);
        return function == other.function;
    }

    void print(std::ostream& os, int /* tabpos */) const override {
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
    }

    void print_sexpr(std::ostream& os, int /* tabpos */) const override {
        switch (function) {
            case AggregateOp::MIN:
            case AggregateOp::FMIN:
            case AggregateOp::UMIN: os << "MIN "; break;
            case AggregateOp::MAX:
            case AggregateOp::UMAX:
            case AggregateOp::FMAX: os << "MAX "; break;
            case AggregateOp::SUM:
            case AggregateOp::FSUM:
            case AggregateOp::USUM: os << "SUM "; break;
            case AggregateOp::COUNT: os << "COUNT "; break;
            case AggregateOp::MEAN: os << "MEAN "; break;
        }
    }

protected:
    /** Aggregation function */
    const AggregateOp function;
};
}  // namespace souffle::ram