/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file GuardedInsert.h
 *
 ***********************************************************************/

#pragma once

#include "ram/ExistenceCheck.h"
#include "ram/Insert.h"
#include "ram/utility/Utils.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class GuardedInsert
 * @brief GuardedInsert a result into the target relation.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     INSERT (t0.a, t0.b, t0.c) INTO @new_X IF (c1 /\ c2 /\ ..)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Where c1, c2 are existenceCheck.
 */

class GuardedInsert : public Insert {
public:
    GuardedInsert(std::string rel, VecOwn<Expression> expressions, Own<Condition> condition = mk<True>())
            : Insert(rel, std::move(expressions)), condition(std::move(condition)) {}

    /** @brief Get guarded condition */
    const Condition* getCondition() const {
        return condition.get();
    }

    GuardedInsert* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->cloning());
        }
        Own<Condition> newCondition(condition->cloning());
        return new GuardedInsert(relation, std::move(newValues), std::move(newCondition));
    }

    void apply(const NodeMapper& map) override {
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
        condition = map(std::move(condition));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "INSERT (" << join(expressions, ", ", print_deref<Own<Expression>>()) << ") INTO " << relation;
        if (!isTrue(condition.get())) {
            os << " IF " << *condition << std::endl;
        } else {
            os << std::endl;
        }
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<GuardedInsert>(node);
        return relation == other.relation && equal_targets(expressions, other.expressions) &&
               equal_ptr(condition, other.condition);
    }

    NodeVec getChildren() const override {
        auto res = Insert::getChildren();
        res.push_back(condition.get());
        return res;
    }

    /* Guarded condition */
    Own<Condition> condition;
};

}  // namespace souffle::ram
