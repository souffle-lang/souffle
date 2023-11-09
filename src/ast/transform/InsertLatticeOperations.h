/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InsertLatticeOperations.h
 *
 * AST transformation related to the support of lattices
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <set>
#include <string>

namespace souffle::ast::transform {

/**
 * Transformation pass to inline marked relations
 */
class LatticeTransformer : public Transformer {
public:
    std::string getName() const override {
        return "InlineRelationsTransformer";
    }

private:
    LatticeTransformer* cloning() const override {
        return new LatticeTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;

    std::map<const ast::QualifiedName, const ast::Lattice*> lattices;
};

}  // namespace souffle::ast::transform
