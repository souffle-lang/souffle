/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Level.h
 *
 * Get level of an expression/condition. The level of a condition/expression
 * determines the outer-most scope in a loop-next of a query,  for which the
 * expression/condition is still safe to be computed.
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Relation.h"

#include <optional>

namespace souffle::ram::analysis {

/**
 * @class LevelAnalysis
 * @brief A Ram Analysis for determining the level of a expression/condition
 *
 * The expression is determined by the TupleElement of an expression/condition
 * with the highest tuple-id number. Note in the implementation we assume that the
 * tuple-id of TupleOperation operations are ordered, i.e., the most-outer loop has the
 * smallest tuple-id and the most inner-loop has the largest tuple-id number.
 *
 * If an expression/condition does not contain an TupleElement accessing an element
 * of a tuple, the analysis yields nullopt for denoting that the expression/condition
 * can be executed outside of the loop-nest, i.e., the expression/condition is
 * independent of data stemming from relations.
 *
 */
class LevelAnalysis : public Analysis {
public:
    LevelAnalysis() : Analysis(name) {}

    static constexpr const char* name = "level-analysis";

    void run(const TranslationUnit& tUnit) override {
        ra = &tUnit.getAnalysis<RelationAnalysis>();
    }

    /**
     * @brief Get level of a RAM expression/condition
     */
    std::optional<std::size_t> getLevel(const Node* value) const;

    bool hasLevel(const Node* value) const;

protected:
    RelationAnalysis* ra{nullptr};
};

}  // namespace souffle::ram::analysis
