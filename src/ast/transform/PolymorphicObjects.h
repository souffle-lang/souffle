/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PolymorphicObjects.h
 *
 * Transformation pass to determine instances of polymorphic object
 * objects = Functors (plus, minus...) ∪ binary constraints (>, ≥ ...) ∪ aggregation ∪ numeric constants
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to determine instances of polymorphic object
 * objects = Functors (plus, minus...) ∪ binary constraints (>, ≥ ...) ∪ aggregation ∪ numeric constants
 */

class PolymorphicObjectsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "PolymorphicObjectsTransformer";
    }

    PolymorphicObjectsTransformer* clone() const override {
        return new PolymorphicObjectsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
