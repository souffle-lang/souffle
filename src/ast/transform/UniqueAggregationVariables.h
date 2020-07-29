/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UniqueAggregationVariables.h
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to rename aggregation variables to make them unique.
 */
class UniqueAggregationVariablesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "UniqueAggregationVariablesTransformer";
    }

    UniqueAggregationVariablesTransformer* clone() const override {
        return new UniqueAggregationVariablesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
