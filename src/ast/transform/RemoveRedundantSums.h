/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveRedundantSums.h
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to remove expressions of the form
 * sum k : { ... } and replace them with
 * k * count : { ... }
 * where k is a constant.
 */
class RemoveRedundantSumsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveRedundantSumsTransformer";
    }

    RemoveRedundantSumsTransformer* clone() const override {
        return new RemoveRedundantSumsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
