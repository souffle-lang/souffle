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
 *     FOR t1 in delta_r ON INDEX t1.0 = t0.1
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    FOR t0 in delta_r
 *     FOR t1 in r ON INDEX t1.0 = t0.1
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * TODO: The same principle applies to IF EXISTS, this transformation could be
 * extended to handle that as well.
 */
class ReorderDelta : public Transformer {
public:
    std::string getName() const override {
        return "ReorderDelta";
    }

    /**
     * @brief reorder filter-break nesting to break-filter nesting
     * @param program Program that is transform
     * @return Flag showing whether the program has been changed by the transformation
     */
    bool reorderDelta(Program& program);

protected:
    bool transform(TranslationUnit& translationUnit) override {
        return reorderDelta(translationUnit.getProgram());
    }
};

}  // namespace souffle::ram::transform
