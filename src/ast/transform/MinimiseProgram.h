/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MinimiseProgram.h
 *
 * Transformation pass to remove equivalent rules.
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/analysis/ClauseNormalisation.h"
#include "ast/transform/Transformer.h"
#include <string>
#include <vector>

namespace souffle::ast {
class Relation;
}

namespace souffle::ast::transform {

/**
 * Transformation pass to remove equivalent rules.
 */
class MinimiseProgramTransformer : public Transformer {
public:
    std::string getName() const override {
        return "MinimiseProgramTransformer";
    }

    // Check whether two normalised clause representations are equivalent.
    static bool areBijectivelyEquivalent(
            const analysis::NormalisedClause& left, const analysis::NormalisedClause& right);

private:
    MinimiseProgramTransformer* cloning() const override {
        return new MinimiseProgramTransformer();
    }

    bool transform(TranslationUnit& translationUnit) override;

    /** -- Bijective Equivalence Helper Methods -- */

    // Check whether a valid variable mapping exists for the given permutation.
    static bool isValidPermutation(const analysis::NormalisedClause& left,
            const analysis::NormalisedClause& right, const std::vector<std::size_t>& permutation);

    // Check whether two relations have the same qualifiers, representation and attribute types.
    static bool areEquivalentRelations(const Relation* firstRelation, const Relation* secondRelation);

    // Checks whether a permutation encoded in the given matrix has a valid corresponding variable mapping.
    static bool existsValidPermutation(const analysis::NormalisedClause& left,
            const analysis::NormalisedClause& right,
            const std::vector<std::vector<std::size_t>>& permutationMatrix);

    /** -- Sub-Transformations -- */

    /**
     * Reduces locally-redundant clauses.
     * A clause is locally-redundant if there is another clause within the same relation
     * that computes the same set of tuples.
     */
    static bool reduceLocallyEquivalentClauses(TranslationUnit& translationUnit);

    /**
     * Remove clauses that are only satisfied if they are already satisfied.
     */
    static bool removeRedundantClauses(TranslationUnit& translationUnit);

    /**
     * Remove redundant literals within a clause.
     */
    static bool reduceClauseBodies(TranslationUnit& translationUnit);

    /**
     * Removes redundant singleton relations.
     * Singleton relations are relations with a single clause. A singleton relation is redundant
     * if there exists another singleton relation that computes the same set of tuples.
     * @return true iff the program was changed
     */
    static bool reduceSingletonRelations(TranslationUnit& translationUnit);
};

}  // namespace souffle::ast::transform
