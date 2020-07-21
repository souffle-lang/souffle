/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTransformer.cpp
 *
 * Defines the interface for AST transformation passes.
 *
 ***********************************************************************/

#include "ast/transform/AstTransformer.h"
#include "ErrorReport.h"
#include "ast/AstTranslationUnit.h"
#include <chrono>
#include <iostream>

namespace souffle {

bool AstTransformer::apply(AstTranslationUnit& translationUnit) {
    // invoke the transformation
    bool changed = transform(translationUnit);

    if (changed) {
        translationUnit.invalidateAnalyses();
    }

    /* Abort evaluation of the program if errors were encountered */
    translationUnit.getErrorReport().exitIfErrors();

    return changed;
}

bool MetaTransformer::applySubtransformer(AstTranslationUnit& translationUnit, AstTransformer* transformer) {
    auto start = std::chrono::high_resolution_clock::now();
    bool changed = transformer->apply(translationUnit);
    auto end = std::chrono::high_resolution_clock::now();

    if (verbose && (dynamic_cast<MetaTransformer*>(transformer) == nullptr)) {
        std::string changedString = changed ? "changed" : "unchanged";
        std::cout << transformer->getName() << " time: " << std::chrono::duration<double>(end - start).count()
                  << "sec [" << changedString << "]" << std::endl;
    }

    return changed;
}

}  // end of namespace souffle
