/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderFilterBreak.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle {

class RamProgram;

/**
 * @class ReorderBreak
 * @brief Reorder filter-break nesting to a break-filter nesting
 *
 * For example ..
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C1
 *     BREAK C2
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    BREAK C2
 *     IF C1
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
class ReorderFilterBreak : public RamTransformer {
public:
    std::string getName() const override {
        return "ReorderFilterBreak";
    }

    /**
     * @brief reorder filter-break nesting to break-filter nesting
     * @param program Program that is transform
     * @return Flag showing whether the program has been changed by the transformation
     */
    bool reorderFilterBreak(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        return reorderFilterBreak(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
