/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveConstantBinaryConstraints.h
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <string>

namespace souffle::ast::transform {

/**
 * Eliminate clauses containing unsatisfiable constant binary constraints
 */
class RemoveConstantBinaryConstraintsTransformer : public Transformer {
public:
    std::string getName() const override {
        return "RemoveConstantBinaryConstraintsTransformer";
    }

private:
    RemoveConstantBinaryConstraintsTransformer* cloning() const override {
        return new RemoveConstantBinaryConstraintsTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;
};

}  // namespace souffle::ast::transform
