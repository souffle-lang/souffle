/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugDeltaRelation.h
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <string>

namespace souffle::ast::transform {

/**
 * Pass that assign the correct signature to relations declared as delta_debug
 */
class DebugDeltaRelationTransformer : public Transformer {
public:
    std::string getName() const override {
        return "DebugDeltaRelationTransformer";
    }

private:
    DebugDeltaRelationTransformer* cloning() const override {
        return new DebugDeltaRelationTransformer();
    }

    bool updateSignature(TranslationUnit& translationUnit);

    bool transform(TranslationUnit& translationUnit) override {
        return updateSignature(translationUnit);
    }
};

}  // namespace souffle::ast::transform
