/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReduceExistentials.h
 *
 * Transformation pass to reduce unnecessary computation for
 * relations that only appear in the form A(_,...,_).
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Transformation pass to reduce unnecessary computation for
 * relations that only appear in the form A(_,...,_).
 */
class ReduceExistentialsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ReduceExistentialsTransformer";
    }

    ReduceExistentialsTransformer* clone() const override {
        return new ReduceExistentialsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
