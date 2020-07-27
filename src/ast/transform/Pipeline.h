/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Pipeline.h
 *
 * Transformer that holds an arbitrary number of sub-transformations
 *
 ***********************************************************************/

#pragma once

#include "DebugReporter.h"
#include "ast/transform/AstTransformer.h"
#include "utility/MiscUtil.h"
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformer that holds an arbitrary number of sub-transformations
 */
class PipelineTransformer : public MetaTransformer {
public:
    template <typename... Args>
    PipelineTransformer(Args... args) {
        std::unique_ptr<AstTransformer> tmp[] = {std::move(args)...};
        for (auto& cur : tmp) {
            pipeline.push_back(std::move(cur));
        }
    }

    PipelineTransformer(std::vector<std::unique_ptr<AstTransformer>> pipeline)
            : pipeline(std::move(pipeline)) {}

    std::vector<AstTransformer*> getSubtransformers() const override {
        return toPtrVector(pipeline);
    }

    void setDebugReport() override {
        for (auto& i : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(i.get())) {
                mt->setDebugReport();
            } else {
                i = std::make_unique<DebugReporter>(std::move(i));
            }
        }
    }

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        for (auto& cur : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(cur.get())) {
                mt->setVerbosity(verbose);
            }
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        for (auto& i : pipeline) {
            if (auto* mt = dynamic_cast<MetaTransformer*>(i.get())) {
                mt->disableTransformers(transforms);
            } else if (transforms.find(i->getName()) != transforms.end()) {
                i = std::make_unique<NullTransformer>();
            }
        }
    }

    std::string getName() const override {
        return "PipelineTransformer";
    }

    PipelineTransformer* clone() const override {
        std::vector<std::unique_ptr<AstTransformer>> transformers;
        for (const auto& transformer : pipeline) {
            transformers.push_back(souffle::clone(transformer));
        }
        return new PipelineTransformer(std::move(transformers));
    }

private:
    std::vector<std::unique_ptr<AstTransformer>> pipeline;
    bool transform(AstTranslationUnit& translationUnit) override {
        bool changed = false;
        for (auto& transformer : pipeline) {
            changed |= applySubtransformer(translationUnit, transformer.get());
        }
        return changed;
    }
};

}  // end of namespace souffle
