/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AbstractIfExists.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/utility/NodeMapper.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AbstractIfExists
 * @brief Abstract class for an if-exists  operation
 *
 * Finding a single tuple, if it exists, such that a condition holds.
 */
class AbstractIfExists {
public:
    AbstractIfExists(Own<Condition> cond) : condition(std::move(cond)) {
        assert(condition != nullptr && "Condition is a null-pointer");
    }

    /** @brief Getter for the condition */
    const Condition& getCondition() const {
        assert(condition != nullptr && "condition of if-exists is a null-pointer");
        return *condition;
    }

    void apply(const NodeMapper& map) {
        condition = map(std::move(condition));
    }

    Node::ConstChildNodes getChildNodes() const {
        return Node::ConstChildNodes(getChildNodesImpl(), detail::RefCaster());
    }

    Node::ChildNodes getChildNodes() {
        return Node::ChildNodes(getChildNodesImpl(), detail::ConstCaster());
    }

protected:
    bool equal(const Node& node) const {
        const auto& other = asAssert<AbstractIfExists, AllowCrossCast>(node);
        return equal_ptr(condition, other.condition);
    }

    std::vector<const Node*> getChildNodesImpl() const {
        return {condition.get()};
    }

    /** Condition for which a tuple in the relation may hold */
    Own<Condition> condition;
};
}  // namespace souffle::ram
