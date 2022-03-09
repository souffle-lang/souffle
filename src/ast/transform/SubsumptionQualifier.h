/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SubsumptionQualifier.h
 *
 * Transformation to change default representation to a "btree_delete"
 * representation in the presence of subsumptive clauses.
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/transform/Transformer.h"
#include <string>

namespace souffle::ast::transform {

class SubsumptionQualifierTransformer : public Transformer {
public:
    std::string getName() const override {
        return "SubsumptionQualifierTransformer";
    }

private:
    SubsumptionQualifierTransformer* cloning() const override {
        return new SubsumptionQualifierTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;
};

}  // namespace souffle::ast::transform
