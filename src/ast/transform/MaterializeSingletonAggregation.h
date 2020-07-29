/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MaterializeSingletonAggregation.h
 *
 * Replaces literals containing single-valued aggregates with
 * a synthesised relation
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <string>

namespace souffle {

class AstAggregator;
class AstClause;
class AstProgram;
class AstTranslationUnit;

/**
 * Replaces literals containing single-valued aggregates with
 * a synthesised relation
 */
class MaterializeSingletonAggregationTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "MaterializeSingletonAggregationTransformer";
    }

    MaterializeSingletonAggregationTransformer* clone() const override {
        return new MaterializeSingletonAggregationTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
    /**
     * Determines whether an aggregate is single-valued,
     * ie the aggregate does not depend on the outer scope.
     */
    static bool isSingleValued(const AstAggregator& agg, const AstClause& clause);
    /**
     * findUniqueVariableName returns a variable name that hasn't appeared
     * in the given clause.
     */
    static std::string findUniqueVariableName(const AstClause& clause);
    /**
     * findUniqueAggregateRelationName returns a synthesised aggregate
     * relation name that hasn't appeared
     * in the given clause.
     */
    static std::string findUniqueAggregateRelationName(const AstProgram& program);
};

}  // end of namespace souffle
