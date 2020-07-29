/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file HoistAggregate.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/analysis/LevelAnalysis.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle {

class RamProgram;

/**
 * @class HoistAggregatesTransformer
 * @brief Pushes one Aggregate as far up the loop nest as possible
 *
 * This transformer, if possible, pushes an aggregate up
 * the loop nest to increase performance by performing less Aggregate
 * operations
 *
 */
class HoistAggregateTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "HoistAggregateTransformer";
    }

    /**
     * @brief Apply hoistAggregate to the whole program
     * @param RAM program
     * @result A flag indicating whether the RAM program has been changed.
     *
     * Pushes an Aggregate up the loop nest if possible
     */
    bool hoistAggregate(RamProgram& program);

protected:
    RamLevelAnalysis* rla{nullptr};
    bool transform(RamTranslationUnit& translationUnit) override {
        rla = translationUnit.getAnalysis<RamLevelAnalysis>();
        return hoistAggregate(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
