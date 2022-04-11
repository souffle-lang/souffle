/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExecutionOrder.h
 *
 * Defines the execution order class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class ExecutionOrder
 * @brief An execution order for atoms within a clause;
 *        one or more execution orders form a plan.
 */
class ExecutionOrder : public Node {
public:
    using ExecOrder = std::vector<std::size_t>;

    ExecutionOrder(ExecOrder order = {}, SrcLocation loc = {});

    /** Get order */
    const ExecOrder& getOrder() const {
        return order;
    }

protected:
    void print(std::ostream& out) const override;

private:
    bool equal(const Node& node) const override;

    ExecutionOrder* cloning() const override;

private:
    /** Literal order of body (starting from 1) */
    ExecOrder order;
};

}  // namespace souffle::ast
