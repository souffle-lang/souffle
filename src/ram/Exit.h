/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Exit.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Statement.h"
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
 * @class Exit
 * @brief Exit statement for a loop
 *
 * Exits a loop if exit condition holds.
 *
 * The following example will exit the loop given
 * that A is the empty set:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * EXIT (A = ∅)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Exit : public Statement {
public:
    Exit(Own<Condition> c) : condition(std::move(c)) {
        assert(condition && "condition is a nullptr");
    }

    /** @brief Get exit condition */
    const Condition& getCondition() const {
        return *condition;
    }

    Exit* cloning() const override {
        return new Exit(clone(condition));
    }

    void apply(const NodeMapper& map) override {
        condition = map(std::move(condition));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "EXIT " << getCondition() << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Exit>(node);
        return equal_ptr(condition, other.condition);
    }

    NodeVec getChildren() const override {
        return {condition.get()};
    }

    /** Exit condition */
    Own<Condition> condition;
};

}  // namespace souffle::ram
