/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstSemanticChecker.h
 *
 * Defines the semantic checker pass.
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

class AstSemanticChecker : public AstTransformer {
public:
    ~AstSemanticChecker() override = default;

    std::string getName() const override {
        return "AstSemanticChecker";
    }

    AstSemanticChecker* clone() const override {
        return new AstSemanticChecker();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

class AstExecutionPlanChecker : public AstTransformer {
public:
    std::string getName() const override {
        return "AstExecutionPlanChecker";
    }

    AstExecutionPlanChecker* clone() const override {
        return new AstExecutionPlanChecker();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

class GroundedTermsChecker : public AstTransformer {
public:
    std::string getName() const override {
        return "GroundedTermsChecker";
    }

    // `apply` but doesn't immediately bail if any errors are found.
    void verify(AstTranslationUnit& translationUnit);

    GroundedTermsChecker* clone() const override {
        return new GroundedTermsChecker();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override {
        verify(translationUnit);
        return false;
    }
};

}  // end of namespace souffle
