/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderConditions.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Program.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Complexity.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle::ram::transform {

/**
 * @class ReorderConditionsTransformer
 * @brief Reorders conjunctive terms depending on cost, i.e.,
 *        cheap terms should be executed first.
 *
 * For example ..
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C(1) /\ C(2) /\ ... /\ C(N) then
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C(i(1)) /\ C(i(2)) /\ ... /\ C(i(N)) then
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  where C(i(1)) <= C(i(2)) <= ....   <= C(i(N)).
 *
 * The terms are sorted according to their complexity class.
 *
 */

class ReorderConditionsTransformer : public Transformer {
public:
    std::string getName() const override {
        return "ReorderConditionsTransformer";
    }

    /**
     * @brief Reorder conjunctive terms in filter operations
     * @param program Program that is transformed
     * @return Flag showing whether the program has been changed
     *         by the transformation
     */
    bool transform(TranslationUnit&) override;
};

}  // namespace souffle::ram::transform
