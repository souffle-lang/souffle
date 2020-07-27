/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstUserDefinedFunctors.h
 *
 * Transformation that passes the type information from user functors
 * declaration to functors instances
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation that passes the type information from user functors
 * declaration to functors instances
 */
class AstUserDefinedFunctorsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "AstUserDefinedFunctorsTransformer";
    }

    AstUserDefinedFunctorsTransformer* clone() const override {
        return new AstUserDefinedFunctorsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};
}  // end of namespace souffle
