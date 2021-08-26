/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "TranslationUnit.h"
#include "Global.h"
#include "souffle/utility/StringUtil.h"

namespace souffle::ram {

void TranslationUnit::logAnalysis(Analysis& analysis) const {
    if (!Global::config().has("debug-report")) return;

    auto ss = toString(analysis);
    debugReport.addSection(
            analysis.getName(), std::string("RAM Analysis ") + analysis.getName(), std::move(ss));
}

}  // namespace souffle::ram
