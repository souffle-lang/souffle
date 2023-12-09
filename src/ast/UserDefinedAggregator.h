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
class UserDefinedAggregator : public Aggregator {
public:
    UserDefinedAggregator(std::string name, Own<Argument> init = {}, Own<Argument> expr = {},
            VecOwn<Literal> body = {}, SrcLocation loc = {});

    std::string getBaseOperatorName() const override {
        return name;
    }

    const Argument* getInit() const {
        return initValue.get();
    }

    void apply(const NodeMapper& map) override;

    static bool classof(const Node*);

protected:
    NodeVec getChildren() const override;

    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    UserDefinedAggregator* cloning() const override;

private:
    /** Aggregate (base type) operator */
    std::string name;

    Own<Argument> initValue;
};

}  // namespace souffle::ast
