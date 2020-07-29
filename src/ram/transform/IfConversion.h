/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IfConversion.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/transform/Transformer.h"
#include <memory>
#include <string>

namespace souffle {

class RamProgram;
class RamIndexScan;
class RamOperation;

/**
 * @class IfConversionTransformer
 * @brief Convert IndexScan operations to Filter/Existence Checks

 * If there exists IndexScan operations in the RAM, and their tuples
 * are not further used in subsequent operations, the IndexScan operations
 * will be rewritten to Filter/Existence Checks.
 *
 * For example,
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	 FOR t1 IN X ON INDEX t1.x = 10 AND t1.y = 20
 *     ... // no occurrence of t1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * will be rewritten to
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF (10,20) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
class IfConversionTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "IfConversionTransformer";
    }

    /**
     * @brief Rewrite IndexScan operations
     * @param indexScan An index operation
     * @result The old operation if the if-conversion fails; otherwise the filter/existence check
     *
     * Rewrites IndexScan operations to a filter/existence check if the IndexScan's tuple
     * is not used in a consecutive RAM operation
     */
    std::unique_ptr<RamOperation> rewriteIndexScan(const RamIndexScan* indexScan);

    /**
     * @brief Apply if-conversion to the whole program
     * @param RAM program
     * @result A flag indicating whether the RAM program has been changed.
     *
     * Search for queries and rewrite their IndexScan operations if possible.
     */
    bool convertIndexScans(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        return convertIndexScans(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
