/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SimplifyConstantBinaryConstraints.h
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <string>

namespace souffle::ast::transform {

/**
 * Rewrites constant binary constraints into boolean constraints
 */
class SimplifyConstantBinaryConstraintsTransformer : public Transformer {
public:
    std::string getName() const override {
        return "SimplifyConstantBinaryConstraintsTransformer";
    }

private:
    SimplifyConstantBinaryConstraintsTransformer* cloning() const override {
        return new SimplifyConstantBinaryConstraintsTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;
};

}  // namespace souffle::ast::transform
