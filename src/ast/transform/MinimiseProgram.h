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

#include "ast/transform/Transformer.h"
#include <string>
#include <vector>

namespace souffle {

class AstClause;
class AstTranslationUnit;

/**
 * Transformation pass to remove equivalent rules.
 */
class MinimiseProgramTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "MinimiseProgramTransformer";
    }

    // Check whether two clauses are bijectively equivalent.
    static bool areBijectivelyEquivalent(const AstClause* left, const AstClause* right);

    MinimiseProgramTransformer* clone() const override {
        return new MinimiseProgramTransformer();
    }

private:
    class NormalisedClauseRepr;

    bool transform(AstTranslationUnit& translationUnit) override;

    /** -- Bijective Equivalence Helper Methods -- */

    // Check whether two normalised clause representations are equivalent.
    static bool areBijectivelyEquivalent(const NormalisedClauseRepr& left, const NormalisedClauseRepr& right);

    // Check whether a valid variable mapping exists for the given permutation.
    static bool isValidPermutation(const NormalisedClauseRepr& left, const NormalisedClauseRepr& right,
            const std::vector<unsigned int>& permutation);

    /** -- Sub-Transformations -- */

    /**
     * Reduces locally-redundant clauses.
     * A clause is locally-redundant if there is another clause within the same relation
     * that computes the same set of tuples.
     */
    static bool reduceLocallyEquivalentClauses(AstTranslationUnit& translationUnit);

    /**
     * Remove clauses that are only satisfied if they are already satisfied.
     */
    static bool removeRedundantClauses(AstTranslationUnit& translationUnit);

    /**
     * Remove redundant literals within a clause.
     */
    static bool reduceClauseBodies(AstTranslationUnit& translationUnit);

    /**
     * Removes redundant singleton relations.
     * Singleton relations are relations with a single clause. A singleton relation is redundant
     * if there exists another singleton relation that computes the same set of tuples.
     * @return true iff the program was changed
     */
    static bool reduceSingletonRelations(AstTranslationUnit& translationUnit);
};

}  // end of namespace souffle
