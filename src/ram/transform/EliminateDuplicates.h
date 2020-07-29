/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file EliminateDuplicates.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle {

class RamProgram;

/**
 * @class EliminateDuplicatesTransformer
 * @brief Eliminates duplicated conjunctive terms
 *
 * For example ..
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C1 /\ C2 /\ ... /\  CN
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF C2 /\ ... /\ CN then
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * assuming that C1 and C2 are equal.
 *
 */
class EliminateDuplicatesTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "EliminateDuplicatesTransformer";
    }

    /**
     * @brief Eliminate duplicated conjunctive terms
     * @param program Program that is transformed
     * @return Flag showing whether the program has been changed by the transformation
     */
    bool eliminateDuplicates(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        return eliminateDuplicates(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
