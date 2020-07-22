/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NameUnnamedVariablesTransformer.h
 *
 * Transformation pass to replace unnamed variables
 * with singletons.
 * E.g.: a() :- b(_). -> a() :- b(x).
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to replace unnamed variables
 * with singletons.
 * E.g.: a() :- b(_). -> a() :- b(x).
 */
class NameUnnamedVariablesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "NameUnnamedVariablesTransformer";
    }

    NameUnnamedVariablesTransformer* clone() const override {
        return new NameUnnamedVariablesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
