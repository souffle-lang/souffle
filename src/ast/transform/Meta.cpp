/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Meta.cpp
 *
 * Defines the interface for AST meta-transformation passes.
 *
 ***********************************************************************/

#include "ast/transform/Meta.h"
#include "souffle/utility/MiscUtil.h"
#include <iostream>

namespace souffle::ast::transform {

bool MetaTransformer::applySubtransformer(TranslationUnit& translationUnit, Transformer* transformer) {
    auto start = now();
    bool changed = transformer->apply(translationUnit);
    auto end = now();

    if (verbose && (!isA<MetaTransformer>(transformer))) {
        std::string changedString = changed ? "changed" : "unchanged";
        const auto elapsed = duration_in_us(start, end);
        std::cout << transformer->getName() << " time: " << std::to_string(elapsed / 1000000.0) << "s ["
                  << changedString << "]" << std::endl;
    }

    return changed;
}

}  // namespace souffle::ast::transform
