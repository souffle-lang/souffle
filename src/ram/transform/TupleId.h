/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TupleId.h
 *
 ***********************************************************************/

#pragma once

#include "ram/TranslationUnit.h"
#include "ram/transform/Transformer.h"
#include <string>

namespace souffle {

class RamProgram;

/**
 * @class TupleIdTransformer
 * @brief Ordering tupleIds in RamTupleOperation operations correctly
 *
 * Transformations, like MakeIndex and IfConversion do not
 * ensure that RamTupleOperations maintain an appropriate order
 * with respect to their tupleId's
 *
 * For example:
 * SEARCH ... (tupleId = 2)
 * ...
 * 		SEARCH ... (tupleId = 1)
 * 			...
 *
 * Will be converted to
 * SEARCH ... (tupleId = 0)
 * ...
 * 		SEARCH ... (tupleId = 1)
 * 			...
 *
 */
class TupleIdTransformer : public RamTransformer {
public:
    std::string getName() const override {
        return "TupleIdTransformer";
    }

    /**
     * @brief Apply tupleId reordering to the whole program
     * @param RAM program
     * @result A flag indicating whether the RAM program has been changed.
     *
     * Search for RamTupleOperations and RamTupleElements and rewrite their tupleIds
     */
    bool reorderOperations(RamProgram& program);

protected:
    bool transform(RamTranslationUnit& translationUnit) override {
        return reorderOperations(translationUnit.getProgram());
    }
};

}  // end of namespace souffle
