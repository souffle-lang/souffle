/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransformer.cpp
 *
 * Defines the interface for RAM transformation passes.
 *
 ***********************************************************************/

#include "RamTransformer.h"
#include "DebugReport.h"
#include "DebugReporter.h"
#include "RamProgram.h"
#include "RamTranslationUnit.h"
#include "Util.h"

#include <algorithm>
#include <sstream>

namespace souffle {

bool RamTransformer::apply(RamTranslationUnit& translationUnit) {
    static const bool debug = Global::config().has("debug-report");
    static const bool verbose = Global::config().has("verbose");
    std::stringstream ramProgStrOld;

    if (debug) {
        ramProgStrOld << translationUnit.getProgram();
    }

    // invoke the transformation
    auto start = std::chrono::high_resolution_clock::now();
    bool changed = transform(translationUnit);
    auto end = std::chrono::high_resolution_clock::now();

    // invalidate analyses in case the program has changed
    if (changed) {
        translationUnit.invalidateAnalyses();
    }

    // print runtime & change info for transformer in verbose mode
    if (verbose && (nullptr == dynamic_cast<RamMetaTransformer*>(this))) {
        std::string changedString = changed ? "changed" : "unchanged";
        std::cout << getName() << " time: " << std::chrono::duration<double>(end - start).count() << "sec ["
                  << changedString << "]" << std::endl;
    }

    // print program after transformation in debug report
    if (debug) {
        translationUnit.getDebugReport().startSection();
        if (changed) {
            translationUnit.getDebugReport().addSection(getName(), "RAM Program after " + getName(),
                    DebugReporter::generateDiff(ramProgStrOld.str(), translationUnit.getProgram()));

            translationUnit.getDebugReport().endSection(getName(), getName());
        } else {
            translationUnit.getDebugReport().endSection(getName(), getName() + " " + " (unchanged)");
        }
    }

    // abort evaluation of the program if errors were encountered
    if (translationUnit.getErrorReport().getNumErrors() != 0) {
        std::cerr << translationUnit.getErrorReport();
        std::cerr << std::to_string(translationUnit.getErrorReport().getNumErrors()) +
                             " errors generated, evaluation aborted"
                  << std::endl;
        exit(1);
    }

    return changed;
}

}  // end of namespace souffle
