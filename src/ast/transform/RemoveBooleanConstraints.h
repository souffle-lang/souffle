/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveBooleanConstraints.h
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to remove constant boolean constraints
 * Should be called after any transformation that may generate boolean constraints
 */
class RemoveBooleanConstraintsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveBooleanConstraintsTransformer";
    }

    RemoveBooleanConstraintsTransformer* clone() const override {
        return new RemoveBooleanConstraintsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
