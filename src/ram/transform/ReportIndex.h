/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReportIndex.h
 *
 ***********************************************************************/

#pragma once

#include "ram/RamOperation.h"
#include "ram/RamTranslationUnit.h"
#include "ram/analysis/RamComplexityAnalysis.h"
#include "ram/analysis/RamIndexAnalysis.h"
#include "ram/analysis/RamLevelAnalysis.h"
#include "ram/transform/RamTransformer.h"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class RamProgram;
class RamCondition;
class RamExpression;

/**
 * @class ReportIndexSetsTransformer
 * @brief does not transform the program but reports on the index sets
 *        if the debug-report flag is enabled.
 *
 */
class ReportIndexTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "ReportIndexTransformer";
    }

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        translationUnit.getAnalysis<RamIndexAnalysis>();
        return false;
    }
};

}  // end of namespace souffle
