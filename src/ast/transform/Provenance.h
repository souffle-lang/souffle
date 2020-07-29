/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Provenance.h
 *
 * Transformation pass to add provenance information
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to add provenance information
 */
class ProvenanceTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ProvenanceTransformer";
    }

    ProvenanceTransformer* clone() const override {
        return new ProvenanceTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
    bool transformMaxHeight(AstTranslationUnit& translationUnit);
};

}  // end of namespace souffle
