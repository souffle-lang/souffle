/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Aggregator.h
 *
 * Defines the aggregator class
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Literal.h"
#include "parser/SrcLocation.h"
#include "souffle/utility/Types.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class Aggregator
 * @brief Defines the aggregator class
 *
 * Example:
 *   sum y+x: {A(y),B(x)}
 *
 * Aggregates over a sub-query using an aggregate operator
 * and an expression.
 */
class IntrinsicAggregator : public Aggregator {
public:
    IntrinsicAggregator(AggregateOp baseOperator, Own<Argument> expr = {}, VecOwn<Literal> body = {},
            SrcLocation loc = {});

    /** Return the (base type) operator of the aggregator */
    AggregateOp getBaseOperator() const {
        return baseOperator;
    }

    std::string getBaseOperatorName() const override {
        std::stringstream s;
        s << baseOperator;
        return s.str();
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    IntrinsicAggregator* cloning() const override;

private:
    /** Aggregate (base type) operator */
    AggregateOp baseOperator;
};

}  // namespace souffle::ast
