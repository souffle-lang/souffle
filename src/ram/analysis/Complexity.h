/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Complexity.h
 *
 * Get the complexity of an expression/condition in terms of
 * database operations. The complexity of an expression/condition is a
 * weighted sum. The weights express the complexity of the terms.
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Relation.h"

namespace souffle::ram::analysis {

/**
 * @class ComplexityAnalysis
 * @brief A Ram Analysis for determining the number of relational
 *        operations in a condition / expression.
 *
 *
 */
class ComplexityAnalysis : public Analysis {
public:
    ComplexityAnalysis() : Analysis(name) {}

    static constexpr const char* name = "complexity-analysis";

    void run(const TranslationUnit& tUnit) override {
        ra = tUnit.getAnalysis<RelationAnalysis>();
    }

    /**
     * @brief Get complexity of a RAM expression/condition
     */
    int getComplexity(const Node* value) const;

protected:
    RelationAnalysis* ra{nullptr};
};

}  // namespace souffle::ram::analysis
