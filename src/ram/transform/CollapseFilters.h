/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CollapseFilters.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle {

class RamProgram;

/**
 * @class CollapseFiltersTransformer
 * @brief Transforms consecutive filters into a single filter containing a conjunction
 *
 * For example ..
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C1
 *     IF C2
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C1 /\ C2 then
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
class CollapseFiltersTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "CollapseFiltersTransformer";
    }

    /**
     * @brief Collapse consecutive filter operations
     * @param program Program that is transformed
     * @return Flag showing whether the program has been changed by the transformation
     */
    bool collapseFilters(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        return collapseFilters(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
