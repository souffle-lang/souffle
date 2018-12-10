/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransforms.h
 *
 * Defines RAM transformation passes.
 *
 ***********************************************************************/

#pragma once

#include "RamTransformer.h"
#include "RamTranslationUnit.h"

namespace souffle {

class RamProgram;

/**
 * Hoists the conditions to the earliest point in the loop nest where they
 * can be evaluated.
 */
class LevelConditionsTransformer : public RamTransformer {
private:
    bool transform(RamTranslationUnit& translationUnit) override {
        return levelConditions(*translationUnit.getProgram());
    }

public:
    std::string getName() const override {
        return "LevelConditionsTransformer";
    }

    /**
     * @param program the program to be processed
     * @return whether the program was modified
     */
    static bool levelConditions(RamProgram& program);
};

}  // end of namespace souffle
