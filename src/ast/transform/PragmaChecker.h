/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PragmaChecker.h
 *
 * Defines a transformer that applies pragmas found in parsed input.
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <functional>
#include <set>
#include <string>

namespace souffle::ast::transform {

class PragmaChecker : public Transformer {
public:
    std::string getName() const override {
        return "PragmaChecker";
    }

    // This helper is used to implement both `--pragma` cmd ln args and `.pragma` statements.
    struct Merger {
        Merger();
        bool operator()(std::string_view key, std::string_view value);

        std::set<std::string, std::less<>> locked_keys;
    };

private:
    PragmaChecker* cloning() const override {
        return new PragmaChecker();
    }

    bool transform(TranslationUnit&) override;
};

}  // namespace souffle::ast::transform
