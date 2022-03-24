/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderDelta.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Program.h"
#include "ram/TranslationUnit.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle::ram::transform {

/**
 * @class ReorderDelta
 * @brief Reorder a scan over a relation followed by a scan over its delta
 *
 * For example ..
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    FOR t0 in r
 *     FOR t1 in delta_r
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    FOR t0 in delta_r
 *     FOR t1 in r
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * For maximal efficacy, this should be run after hoisting conditionals so that
 * more such scans will be next to one another.
 */
class ReorderDelta : public Transformer {
public:
    std::string getName() const override {
        return "ReorderDelta";
    }

    /**
     * @brief reorder a scan over a relation followed by a scan over its delta
     * @param program Program that is to be transformed
     * @return Flag showing whether the program has been changed by the transformation
     */
    bool reorderDelta(Program& program);

protected:
    bool transform(TranslationUnit& translationUnit) override {
        return reorderDelta(translationUnit.getProgram());
    }
};

}  // namespace souffle::ram::transform
