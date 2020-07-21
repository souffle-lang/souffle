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

#include <set>
#include <string>
#include <vector>

namespace souffle {

class AstTranslationUnit;

class AstTransformer {
private:
    virtual bool transform(AstTranslationUnit& translationUnit) = 0;

public:
    virtual ~AstTransformer() = default;

    bool apply(AstTranslationUnit& translationUnit);

    virtual std::string getName() const = 0;

    virtual AstTransformer* clone() const = 0;
};

/**
 * Transformer that coordinates other sub-transformations
 */
class MetaTransformer : public AstTransformer {
protected:
    bool verbose = false;

public:
    /* Get subtransformers */
    virtual std::vector<AstTransformer*> getSubtransformers() const = 0;

    /* Enable the debug-report for all sub-transformations */
    virtual void setDebugReport() = 0;

    /* Enable high verbosity */
    virtual void setVerbosity(bool verbose) = 0;

    /* Disable subtransformers */
    virtual void disableTransformers(const std::set<std::string>& transforms) = 0;

    /* Apply a nested transformer */
    bool applySubtransformer(AstTranslationUnit& translationUnit, AstTransformer* transformer);

    MetaTransformer* clone() const override = 0;
};

/**
 * Transformer that does absolutely nothing
 */
class NullTransformer : public MetaTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::vector<AstTransformer*> getSubtransformers() const override {
        return {};
    }

    void setDebugReport() override {}

    void setVerbosity(bool /* verbose */) override {}

    void disableTransformers(const std::set<std::string>& /* transforms */) override {}

    std::string getName() const override {
        return "NullTransformer";
    }

    NullTransformer* clone() const override {
        return new NullTransformer();
    }
};

}  // end of namespace souffle
