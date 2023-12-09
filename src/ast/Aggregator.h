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
class Aggregator : public Argument {
public:
    Aggregator(NodeKind Kind, Own<Argument> expr = {}, VecOwn<Literal> body = {}, SrcLocation loc = {});

    /** Return target expression */
    const Argument* getTargetExpression() const {
        return targetExpression.get();
    }

    Argument* getTargetExpression() {
        return targetExpression.get();
    }

    /** Return body literals */
    std::vector<Literal*> getBodyLiterals() const;

    /** Set body literals, returns previous body literals */
    VecOwn<Literal> setBodyLiterals(VecOwn<Literal> bodyLiterals);

    void apply(const NodeMapper& map) override;

    virtual std::string getBaseOperatorName() const = 0;

    static bool classof(const Node*);

protected:
    NodeVec getChildren() const override;

    /** Aggregate expression */
    Own<Argument> targetExpression;

    /** Body literal of sub-query */
    VecOwn<Literal> body;
};

}  // namespace souffle::ast
