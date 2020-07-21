/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamUtils.h
 *
 * A collection of utilities operating on RAM constructs.
 *
 ***********************************************************************/

#pragma once

#include "ram/RamCondition.h"
#include "ram/RamExpression.h"
#include <algorithm>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

namespace souffle {

/** @brief Determines if an expression represents an undefined value */
inline bool isRamUndefValue(const RamExpression* expr) {
    return nullptr != dynamic_cast<const RamUndefValue*>(expr);
}

/** @brief Determines if a condition represents true */
inline bool isRamTrue(const RamCondition* cond) {
    return nullptr != dynamic_cast<const RamTrue*>(cond);
}

/**
 * @brief Convert terms of a conjunction to a list
 * @param conds A RAM condition
 * @return A list of RAM conditions
 *
 * Convert a condition of the format C1 /\ C2 /\ ... /\ Cn
 * to a list {C1, C2, ..., Cn}.
 */
inline std::vector<std::unique_ptr<RamCondition>> toConjunctionList(const RamCondition* condition) {
    std::vector<std::unique_ptr<RamCondition>> conditionList;
    std::queue<const RamCondition*> conditionsToProcess;
    if (condition != nullptr) {
        conditionsToProcess.push(condition);
        while (!conditionsToProcess.empty()) {
            condition = conditionsToProcess.front();
            conditionsToProcess.pop();
            if (const auto* ramConj = dynamic_cast<const RamConjunction*>(condition)) {
                conditionsToProcess.push(&ramConj->getLHS());
                conditionsToProcess.push(&ramConj->getRHS());
            } else {
                conditionList.emplace_back(condition->clone());
            }
        }
    }
    return conditionList;
}

/**
 * @brief Convert list of conditions to a conjunction
 * @param A list of RAM conditions
 * @param A RAM condition
 *
 * Convert a list {C1, C2, ..., Cn} to a condition of
 * the format C1 /\ C2 /\ ... /\ Cn.
 */
inline std::unique_ptr<RamCondition> toCondition(const std::vector<std::unique_ptr<RamCondition>>& conds) {
    std::unique_ptr<RamCondition> result;
    for (auto const& cur : conds) {
        if (result == nullptr) {
            result = souffle::clone(cur);
        } else {
            result = std::make_unique<RamConjunction>(std::move(result), souffle::clone(cur));
        }
    }
    return result;
}

}  // end of namespace souffle
