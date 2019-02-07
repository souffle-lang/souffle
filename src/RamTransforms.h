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

#include "RamConditionLevel.h"
#include "RamConstValue.h"
#include "RamTransformer.h"
#include "RamTranslationUnit.h"
#include "RamValueLevel.h"

namespace souffle {

class RamProgram;

/**
 * Hoists the conditions to the earliest point in the loop nest where they
 * can be evaluated.
 */
class LevelConditionsTransformer : public RamTransformer {
    RamConditionLevelAnalysis* rcla{nullptr};

private:
    bool transform(RamTranslationUnit& translationUnit) override {
        rcla = translationUnit.getAnalysis<RamConditionLevelAnalysis>();
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
    bool levelConditions(RamProgram& program);
};

class CreateIndicesTransformer : public RamTransformer {
    RamConstValueAnalysis* rcva{nullptr};
    RamValueLevelAnalysis* rvla{nullptr};

private:
    bool transform(RamTranslationUnit& translationUnit) override {
        rcva = translationUnit.getAnalysis<RamConstValueAnalysis>();
        rvla = translationUnit.getAnalysis<RamValueLevelAnalysis>();
        return createIndices(*translationUnit.getProgram());
    }

public:
    std::string getName() const override {
        return "CreateIndicesTransformer";
    }

    std::unique_ptr<RamValue> getIndexElement(RamCondition* c, size_t& element, size_t level);

    std::unique_ptr<RamOperation> rewriteScan(const RamScan* scan);

    /**
     * @param program the program to be processed
     * @return whether the program was modified
     */
    bool createIndices(RamProgram& program);
};

class ConvertExistenceChecksTransformer : public RamTransformer {
    RamConstValueAnalysis* rcva{nullptr};
    RamConditionLevelAnalysis* rcla{nullptr};
    RamValueLevelAnalysis* rvla{nullptr};

private:
    bool transform(RamTranslationUnit& translationUnit) override {
        rcva = translationUnit.getAnalysis<RamConstValueAnalysis>();
        rcla = translationUnit.getAnalysis<RamConditionLevelAnalysis>();
        rvla = translationUnit.getAnalysis<RamValueLevelAnalysis>();
        return convertExistenceChecks(*translationUnit.getProgram());
    }

public:
    std::string getName() const override {
        return "ConvertExistenceChecksTransformer";
    }

    /**
     * @param program the program to be processed
     * @return whether the program was modified
     */
    bool convertExistenceChecks(RamProgram& program);
};

}  // end of namespace souffle
