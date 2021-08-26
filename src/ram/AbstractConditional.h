/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AbstractConditional.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/utility/NodeMapper.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AbstractConditional
 * @brief Abstract conditional statement
 */
class AbstractConditional : public NestedOperation {
public:
    AbstractConditional(Own<Condition> cond, Own<Operation> nested, std::string profileText = "")
            : NestedOperation(std::move(nested), std::move(profileText)), condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
    }

    AbstractConditional* cloning() const override = 0;

    /** @brief Get condition that must be satisfied */
    const Condition& getCondition() const {
        assert(condition != nullptr && "condition of conditional operation is a null-pointer");
        return *condition;
    }

    void apply(const NodeMapper& map) override {
        NestedOperation::apply(map);
        condition = map(std::move(condition));
    }

protected:
    bool equal(const Node& node) const override {
        const auto& other = asAssert<AbstractConditional>(node);
        return NestedOperation::equal(node) && equal_ptr(condition, other.condition);
    }

    NodeVec getChildren() const override {
        auto res = NestedOperation::getChildren();
        res.push_back(condition.get());
        return res;
    }

    /** Condition */
    Own<Condition> condition;
};

}  // namespace souffle::ram
