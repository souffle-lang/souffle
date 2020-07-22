/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InlineRelationsTransformer.h
 *
 * Transformation pass to inline marked relations
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to inline marked relations
 */
class InlineRelationsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "InlineRelationsTransformer";
    }

    InlineRelationsTransformer* clone() const override {
        return new InlineRelationsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
