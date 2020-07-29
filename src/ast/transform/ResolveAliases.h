/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ResolveAliases.h
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/Transformer.h"
#include <memory>
#include <string>

namespace souffle {

class AstClause;
class AstTranslationUnit;

/**
 * Transformation pass to eliminate grounded aliases.
 * e.g. resolve: a(r) , r = [x,y]       => a(x,y)
 * e.g. resolve: a(x) , !b(y) , y = x   => a(x) , !b(x)
 */
class ResolveAliasesTransformer : public AstTransformer {
public:
    std::string getName() const override {
        return "ResolveAliasesTransformer";
    }

    /**
     * Converts the given clause into a version without variables aliasing
     * grounded variables.
     *
     * @param clause the clause to be processed
     * @return a modified clone of the processed clause
     */
    static std::unique_ptr<AstClause> resolveAliases(const AstClause& clause);

    /**
     * Removes trivial equalities of the form t = t from the given clause.
     *
     * @param clause the clause to be processed
     * @return a modified clone of the given clause
     */
    static std::unique_ptr<AstClause> removeTrivialEquality(const AstClause& clause);

    /**
     * Removes complex terms in atoms, replacing them with constrained variables.
     *
     * @param clause the clause to be processed
     * @return a modified clone of the processed clause
     */
    static std::unique_ptr<AstClause> removeComplexTermsInAtoms(const AstClause& clause);

    ResolveAliasesTransformer* clone() const override {
        return new ResolveAliasesTransformer();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;
};

}  // end of namespace souffle
