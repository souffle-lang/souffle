/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTuner.h
 *
 * Provides utilities to conduct automated tuning of AstStructures.
 *
 ***********************************************************************/

#pragma once

#include "AstTransformer.h"
#include "Global.h"

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass which tunes the given program by re-ordering the atoms
 * in the included rules to improve performance.
 *
 * This operation is based on an actual, interpreted execution of the
 * given program and the collection of profiling data.
 */
class AutoScheduleTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    AutoScheduleTransformer() {}

    ~AutoScheduleTransformer() override = default;

    std::string getName() const override {
        return "AutoScheduleTransformer";
    }

    /**
     * Perform auto-scheduling for the given program.
     * @return whether the program was modified
     */
    static bool autotune(AstTranslationUnit& translationUnit, std::ostream* report);
};

}  // end of namespace souffle
