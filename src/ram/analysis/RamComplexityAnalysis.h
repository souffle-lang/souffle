/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamComplexityAnalysis.h
 *
 * Get the complexity of an expression/condition in terms of
 * database operations. The complexity of an expression/condition is a
 * weighted sum. The weights express the complexity of the terms.
 ***********************************************************************/

#pragma once

#include "ram/analysis/RamAnalysis.h"

namespace souffle {

class RamNode;
class RamTranslationUnit;

/**
 * @class RamComplexityAnalysis
 * @brief A Ram Analysis for determining the number of relational
 *        operations in a condition / expression.
 *
 *
 */
class RamComplexityAnalysis : public RamAnalysis {
public:
    RamComplexityAnalysis(const char* id) : RamAnalysis(id) {}

    static constexpr const char* name = "complexity-analysis";

    void run(const RamTranslationUnit&) override {}

    /**
     * @brief Get complexity of a RAM expression/condition
     */
    int getComplexity(const RamNode* value) const;
};

}  // end of namespace souffle
