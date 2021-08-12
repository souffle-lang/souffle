/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Query.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Query
 * @brief A relational algebra query
 *
 * Corresponds to the core machinery of semi-naive evaluation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * QUERY
 *   FOR t0 in A
 *     FOR t1 in B
 *       ...
 * END QUERY
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Query : public Statement {
public:
    Query(Own<Operation> o) : operation(std::move(o)) {
        assert(operation && "operation is a nullptr");
    }

    /** @brief Get RAM operation */
    const Operation& getOperation() const {
        return *operation;
    }

    Query* cloning() const override {
        return new Query(clone(operation));
    }

    void apply(const NodeMapper& map) override {
        operation = map(std::move(operation));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "QUERY" << std::endl;
        operation->print(os, tabpos + 1);
        os << times(" ", tabpos) << "END QUERY" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Query>(node);
        return equal_ptr(operation, other.operation);
    }

    NodeVec getChildNodesImpl() const override {
        return {operation.get()};
    }

    /** RAM operation */
    Own<Operation> operation;
};

}  // namespace souffle::ram
