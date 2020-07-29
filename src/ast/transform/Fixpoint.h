/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Fixpoint.h
 *
 * Transformer that repeatedly executes a sub-transformer until no changes are made
 *
 ***********************************************************************/

#pragma once

#include "DebugReporter.h"
#include "ast/transform/Transformer.h"
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformer that repeatedly executes a sub-transformer until no changes are made
 */
class FixpointTransformer : public MetaTransformer {
public:
    FixpointTransformer(std::unique_ptr<AstTransformer> transformer) : transformer(std::move(transformer)) {}

    void setDebugReport() override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setDebugReport();
        } else {
            transformer = std::make_unique<DebugReporter>(std::move(transformer));
        }
    }

    std::vector<AstTransformer*> getSubtransformers() const override {
        return {transformer.get()};
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setVerbosity(verbose);
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->disableTransformers(transforms);
        } else if (transforms.find(transformer->getName()) != transforms.end()) {
            transformer = std::make_unique<NullTransformer>();
        }
    }

    std::string getName() const override {
        return "FixpointTransformer";
    }

    FixpointTransformer* clone() const override {
        return new FixpointTransformer(souffle::clone(transformer));
    }

private:
    std::unique_ptr<AstTransformer> transformer;
    bool transform(AstTranslationUnit& translationUnit) override {
        bool changed = false;
        while (applySubtransformer(translationUnit, transformer.get())) {
            changed = true;
        }
        return changed;
    }
};

}  // end of namespace souffle
