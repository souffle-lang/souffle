/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugReporter.cpp
 *
 * Defines class for adapting other transformers to produce debug output
 *
 ***********************************************************************/

#include "ast/transform/DebugReporter.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Utils.h"
#include "reports/DebugReport.h"
#include "souffle/utility/MiscUtil.h"

#include <string>

namespace souffle::ast::transform {

bool DebugReporter::transform(TranslationUnit& translationUnit) {
    translationUnit.getDebugReport().startSection();
    auto datalogSpecOriginal = pprint(translationUnit.getProgram());
    auto start = now();
    bool changed = applySubtransformer(translationUnit, wrappedTransformer.get());
    auto end = now();

    if (changed) {
        generateDebugReport(translationUnit, datalogSpecOriginal);
    }

    const auto elapsed = duration_in_us(start, end);
    translationUnit.getDebugReport().endSection(wrappedTransformer->getName(),
            wrappedTransformer->getName() + " (" + std::to_string(elapsed / 1000000.0) + "s)" +
                    (changed ? "" : " (unchanged)"));
    return changed;
}

void DebugReporter::generateDebugReport(TranslationUnit& tu, const std::string& preTransformDatalog) {
    tu.getDebugReport().addCodeSection(
            "dl", "Datalog", "souffle", preTransformDatalog, pprint(tu.getProgram()));
}

}  // namespace souffle::ast::transform
