/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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

#include "ast/Argument.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/NodeMapper.h"
#include "parser/AggregateOp.h"
#include "parser/SrcLocation.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class AstAggregator
 * @brief Defines the aggregator class
 *
 * Example:
 *   sum y+x: {A(y),B(x)}
 *
 * Aggregates over a sub-query using an aggregate operator
 * and an expression.
 */
class AstAggregator : public AstArgument {
public:
    AstAggregator(AggregateOp fun, Own<AstArgument> expr = nullptr, VecOwn<AstLiteral> body = {},
            SrcLocation loc = {})
            : AstArgument(std::move(loc)), fun(fun), targetExpression(std::move(expr)),
              body(std::move(body)) {}

    /** Return aggregate operator */
    AggregateOp getOperator() const {
        return fun;
    }

    /** Set aggregate operator */
    void setOperator(AggregateOp op) {
        fun = op;
    }

    /** Return target expression */
    const AstArgument* getTargetExpression() const {
        return targetExpression.get();
    }

    /** Return body literals */
    std::vector<AstLiteral*> getBodyLiterals() const {
        return toPtrVector(body);
    }

    /** Set body */
    void setBody(VecOwn<AstLiteral> bodyLiterals) {
        body = std::move(bodyLiterals);
    }

    std::vector<const AstNode*> getChildNodes() const override {
        auto res = AstArgument::getChildNodes();
        if (targetExpression) {
            res.push_back(targetExpression.get());
        }
        for (auto& cur : body) {
            res.push_back(cur.get());
        }
        return res;
    }

    AstAggregator* clone() const override {
        return new AstAggregator(fun, souffle::clone(targetExpression), souffle::clone(body), getSrcLoc());
    }

    void apply(const AstNodeMapper& map) override {
        if (targetExpression) {
            targetExpression = map(std::move(targetExpression));
        }
        for (auto& cur : body) {
            cur = map(std::move(cur));
        }
    }

protected:
    void print(std::ostream& os) const override {
        os << fun;
        if (targetExpression) {
            os << " " << *targetExpression;
        }
        os << " : { " << join(body) << " }";
    }

    bool equal(const AstNode& node) const override {
        const auto& other = static_cast<const AstAggregator&>(node);
        return fun == other.fun && equal_ptr(targetExpression, other.targetExpression) &&
               equal_targets(body, other.body);
    }

private:
    /** Aggregate operator */
    AggregateOp fun;

    /** Aggregate expression */
    Own<AstArgument> targetExpression;

    /** Body literal of sub-query */
    VecOwn<AstLiteral> body;
};

}  // end of namespace souffle
