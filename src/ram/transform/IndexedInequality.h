/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IndexedInequality.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/analysis/IndexAnalysis.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle {

class RamProgram;

/**
 * @class IndexedInequalityTransformer
 * @brief Removes Inequalities from Indexed Operations and replaces them with a Filter Operation
 * and empty Indexed Operations are coverted to their Non-Indexed semantic equivalent
 *
 * If there exists inequality constraints in an Indexed Operation, these constraints will be
 * replaced with a semantically equivalent nested Filter Operation.
 *
 * Furthermore, if after removing all of these inequality constraints from the Indexed Operation
 * we may find that the Indexed Operation is empty (no constraints).
 * This occurs in the case where an Indexed Operation is composed entirely of inequality constraints.
 * In this situation, the Indexed Operation is empty and replaced with a semantically equivalent Operation.
 * i.e. RamIndexScan -> RamScan
 *
 * For example,
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	 FOR t1 IN X ON INDEX t1.x < 10 AND t1.y > 20
 *     ... // t1 only has inequality constraints placed on it
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *       FOR t1 in X ON INDEX // empty index since all inequalities have been removed
 *            IF t1.x < 10 AND t1.y > 20
 *                // replaced with a semantically equivalent filter
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *       FOR t1 in X // RamScan instead of RamIndexScan
 *            IF t1.x < 10 AND t1.y > 20
 *                // replaced with a semantically equivalent filter
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
class IndexedInequalityTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "IndexedInequalityTransformer";
    }

    /** Converts a box query into a corresponding partial box query operation.
     *  This will turn every box query into a filter operation.
     */
    bool transformIndexToFilter(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        idxAnalysis = translationUnit.getAnalysis<RamIndexAnalysis>();
        return transformIndexToFilter(translationUnit.getProgram());
    }

    RamIndexAnalysis* idxAnalysis;
};

}  // end of namespace souffle
