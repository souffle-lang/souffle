/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTransformer.h
 *
 * Defines the interface for AST transformation passes.
 *
 ***********************************************************************/
#pragma once

#include <string>

namespace souffle {

class AstTranslationUnit;

class AstTransformer {
private:
    virtual bool transform(AstTranslationUnit& translationUnit) = 0;

public:
    virtual ~AstTransformer() = default;

    bool apply(AstTranslationUnit& translationUnit);

    virtual std::string getName() const = 0;
};

}  // end of namespace souffle
