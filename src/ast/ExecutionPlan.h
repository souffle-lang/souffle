/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExecutionPlan.h
 *
 * Defines an execution plan class
 *
 ***********************************************************************/

#pragma once

#include "ast/ExecutionOrder.h"
#include "ast/Node.h"
#include <iosfwd>
#include <map>

namespace souffle::ast {

/**
 * @brief ExecutionPlan
 * @class Defines a user-defined execution plan for a clause.
 *
 * An user-defined execution plan consists of one or more
 * execution orders. An execution order is a permutation
 * of atoms in a clause.
 *
 * Example:
 *   .plan 0:(1,2,3), 2:(3,2,1)
 *
 */
class ExecutionPlan : public Node {
public:
    using Node::Node;

    /** Set execution order for a given rule version */
    void setOrderFor(std::size_t version, Own<ExecutionOrder> plan);

    /** Get orders */
    std::map<std::size_t, const ExecutionOrder*> getOrders() const;

    void apply(const NodeMapper& map) override;

    NodeVec getChildren() const override;

protected:
    void print(std::ostream& out) const override;

private:
    bool equal(const Node& node) const override;

    ExecutionPlan* cloning() const override;

private:
    /** Mapping versions of clauses to execution orders */
    std::map<std::size_t, Own<ExecutionOrder>> plans;
};

}  // namespace souffle::ast
