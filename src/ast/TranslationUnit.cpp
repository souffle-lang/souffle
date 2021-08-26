/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/TranslationUnit.h"
#include "Global.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "ast/analysis/SCCGraph.h"
#include "reports/DebugReport.h"
#include "souffle/utility/StringUtil.h"

namespace souffle::ast {

/** get analysis: analysis is generated on the fly if not present */
void TranslationUnit::logAnalysis(Analysis& analysis) const {
    if (!Global::config().has("debug-report")) return;

    std::string name = analysis.getName();
    if (as<analysis::PrecedenceGraphAnalysis>(analysis) || as<analysis::SCCGraphAnalysis>(analysis)) {
        debugReport.addSection(
                DebugReportSection(name, "Ast Analysis [" + name + "]", {}, toString(analysis)));
    } else {
        debugReport.addSection(name, "Ast Analysis [" + name + "]", toString(analysis));
    }
}

}  // namespace souffle::ast
