/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentInstantiationTransformer.h
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {
class AstTranslationUnit;

class ComponentInstantiationTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ComponentInstantiationTransformer";
    }

    ComponentInstantiationTransformer* clone() const override {
        return new ComponentInstantiationTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
