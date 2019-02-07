/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamConditionLevel.h
 *
 * Get level of condition (which for-loop of a query)
 *
 ***********************************************************************/

#pragma once

#include "RamAnalysis.h"
#include "RamCondition.h"
#include "RamValueLevel.h"

namespace souffle {

/*
 * Class for a level analysis
 */
class RamConditionLevelAnalysis : public RamAnalysis {
    RamValueLevelAnalysis* rvla;

public:
    /** name of analysis */
    static constexpr const char* name = "condition-level-analysis";

    /** run level analysis for a RAM translation unit */
    void run(const RamTranslationUnit& translationUnit) override;

    /** Get level */
    size_t getLevel(const RamCondition* condition) const;
};

}  // end of namespace souffle
