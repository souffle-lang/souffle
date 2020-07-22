/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ConditionalTransformer.h
 *
 * Transformer that executes a sub-transformer iff a condition holds
 *
 ***********************************************************************/

#pragma once

#include "DebugReporter.h"
#include "ast/transform/AstTransformer.h"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformer that executes a sub-transformer iff a condition holds
 */
class ConditionalTransformer : public MetaTransformer {
public:
    ConditionalTransformer(std::function<bool()> cond, std::unique_ptr<AstTransformer> transformer)
            : condition(std::move(cond)), transformer(std::move(transformer)) {}

    ConditionalTransformer(bool cond, std::unique_ptr<AstTransformer> transformer)
            : condition([=]() { return cond; }), transformer(std::move(transformer)) {}

    std::vector<AstTransformer*> getSubtransformers() const override {
        return {transformer.get()};
    }

    void setDebugReport() override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(transformer.get())) {
            mt->setDebugReport();
        } else {
            transformer = std::make_unique<DebugReporter>(std::move(transformer));
        }
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
        return "ConditionalTransformer";
    }

    ConditionalTransformer* clone() const override {
        return new ConditionalTransformer(condition, souffle::clone(transformer));
    }

private:
    std::function<bool()> condition;
    std::unique_ptr<AstTransformer> transformer;

    bool transform(AstTranslationUnit& translationUnit) override {
        return condition() ? applySubtransformer(translationUnit, transformer.get()) : false;
    }
};

}  // end of namespace souffle
