/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveEmptyRelationsTransformer.h
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {

class AstRelation;
class AstTranslationUnit;

/**
 * Transformation pass to remove all empty relations and rules that use empty relations.
 */
class RemoveEmptyRelationsTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "RemoveEmptyRelationsTransformer";
    }

    /**
     * Eliminate all empty relations (and their uses) in the given program.
     *
     * @param translationUnit the program to be processed
     * @return whether the program was modified
     */
    static bool removeEmptyRelations(AstTranslationUnit& translationUnit);

    RemoveEmptyRelationsTransformer* clone() const override {
        return new RemoveEmptyRelationsTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override {
        return removeEmptyRelations(translationUnit);
    }

    /**
     * Eliminate rules that contain empty relations and/or rewrite them.
     *
     * @param translationUnit the program to be processed
     * @param emptyRelation relation that is empty
     * @return whether the program was modified
     */
    static bool removeEmptyRelationUses(AstTranslationUnit& translationUnit, AstRelation* emptyRelation);
};

}  // end of namespace souffle
