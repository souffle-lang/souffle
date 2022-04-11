/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.h
 *
 * A collection of utilities operating on RAM constructs.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Expression.h"
#include "ram/True.h"
#include "ram/UndefValue.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

namespace souffle::ram {

/** @brief Determines if an expression represents an undefined value */
inline bool isUndefValue(const Expression* expr) {
    return isA<UndefValue>(expr);
}

/** @brief Determines if a condition represents true */
inline bool isTrue(const Condition* cond) {
    return isA<True>(cond);
}

/**
 * @brief Convert terms of a conjunction to a list
 * @param conds A RAM condition
 * @return A list of RAM conditions
 *
 * Convert a condition of the format C1 /\ C2 /\ ... /\ Cn
 * to a list {C1, C2, ..., Cn}.
 */
inline VecOwn<Condition> toConjunctionList(const Condition* condition) {
    VecOwn<Condition> conditionList;
    std::queue<const Condition*> conditionsToProcess;
    if (condition != nullptr) {
        conditionsToProcess.push(condition);
        while (!conditionsToProcess.empty()) {
            condition = conditionsToProcess.front();
            conditionsToProcess.pop();
            if (const auto* ramConj = as<Conjunction>(condition)) {
                conditionsToProcess.push(&ramConj->getLHS());
                conditionsToProcess.push(&ramConj->getRHS());
            } else {
                conditionList.emplace_back(condition->cloning());
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
inline Own<Condition> toCondition(const VecOwn<Condition>& conds) {
    Own<Condition> result;
    for (auto const& cur : conds) {
        if (result == nullptr) {
            result = clone(cur);
        } else {
            result = mk<Conjunction>(std::move(result), clone(cur));
        }
    }
    if (result == nullptr) {
        result = mk<True>();
    }
    return result;
}

/**
 * @brief store terms of a conjunction in an array of pointers without cloning
 *
 * Convert a condition of the format C1 /\ C2 /\ ... /\ Cn
 * to a list {C1, C2, ..., Cn}.
 */
inline std::vector<const ram::Condition*> findConjunctiveTerms(const ram::Condition* condition) {
    std::vector<const ram::Condition*> conditionList;
    std::queue<const ram::Condition*> conditionsToProcess;
    if (condition != nullptr) {
        conditionsToProcess.push(condition);
        while (!conditionsToProcess.empty()) {
            condition = conditionsToProcess.front();
            conditionsToProcess.pop();
            if (const auto* ramConj = as<ram::Conjunction>(condition)) {
                conditionsToProcess.push(&ramConj->getLHS());
                conditionsToProcess.push(&ramConj->getRHS());
            } else {
                conditionList.emplace_back(condition);
            }
        }
    }
    return conditionList;
}

}  // namespace souffle::ram
