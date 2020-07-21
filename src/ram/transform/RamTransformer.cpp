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

#include "ram/transform/RamTransformer.h"
#include "DebugReport.h"
#include "ErrorReport.h"
#include "Global.h"
#include "ram/RamNode.h"
#include "ram/RamProgram.h"
#include "ram/RamTranslationUnit.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace souffle {

bool RamTransformer::apply(RamTranslationUnit& translationUnit) {
    const bool debug = Global::config().has("debug-report");
    const bool verbose = Global::config().has("verbose");
    std::string ramProgStrOld = debug ? toString(translationUnit.getProgram()) : "";

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
            translationUnit.getDebugReport().addCodeSection(getName(), "RAM Program after " + getName(),
                    "ram", ramProgStrOld, toString(translationUnit.getProgram()));

            translationUnit.getDebugReport().endSection(getName(), getName());
        } else {
            translationUnit.getDebugReport().endSection(getName(), getName() + " " + " (unchanged)");
        }
    }

    // abort evaluation of the program if errors were encountered
    if (translationUnit.getErrorReport().getNumErrors() != 0) {
        std::cerr << translationUnit.getErrorReport();
        std::cerr << translationUnit.getErrorReport().getNumErrors()
                  << " errors generated, evaluation aborted" << std::endl;
        exit(EXIT_FAILURE);
    }

    return changed;
}

}  // end of namespace souffle
