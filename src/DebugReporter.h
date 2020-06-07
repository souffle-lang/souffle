/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugReporter.h
 *
 * Defines an adaptor transformer to capture debug output from other transformers
 *
 ***********************************************************************/
#pragma once

#include "AstTransformer.h"
#include "utility/FileUtil.h"
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass which wraps another transformation pass and generates
 * a debug report section for the stage after applying the wrapped transformer,
 * and adds it to the translation unit's debug report.
 */
class DebugReporter : public MetaTransformer {
public:
    DebugReporter(std::unique_ptr<AstTransformer> wrappedTransformer)
            : wrappedTransformer(std::move(wrappedTransformer)) {}

    void setDebugReport() override {}

    void setVerbosity(bool verbose) override {
        this->verbose = verbose;
        if (auto* mt = dynamic_cast<MetaTransformer*>(wrappedTransformer.get())) {
            mt->setVerbosity(verbose);
        }
    }

    void disableTransformers(const std::set<std::string>& transforms) override {
        if (auto* mt = dynamic_cast<MetaTransformer*>(wrappedTransformer.get())) {
            mt->disableTransformers(transforms);
        } else if (transforms.find(wrappedTransformer->getName()) != transforms.end()) {
            wrappedTransformer = std::unique_ptr<AstTransformer>(new NullTransformer());
        }
    }

    std::string getName() const override {
        return "DebugReporter";
    }

private:
    std::unique_ptr<AstTransformer> wrappedTransformer;

    bool transform(AstTranslationUnit& translationUnit) override;

    void generateDebugReport(AstTranslationUnit& tu, const std::string& preTransformDatalog);
};

}  // end of namespace souffle
