/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AuxArityAnalysis.h
 *
 * Define of AST analyses classes
 *
 ***********************************************************************/

#pragma once

#include "ast/AstLiteral.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/analysis/AstAnalysis.h"
#include <cstddef>
#include <string>

namespace souffle {
class AstRelation;
class AstProgram;

/**
 * Determine the auxiliary arity for relations
 */
class AuxiliaryArity : public AstAnalysis {
public:
    static constexpr const char* name = "auxiliary-arity";

    AuxiliaryArity() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override {
        program = translationUnit.getProgram();
    }

    /**
     * Returns the number of auxiliary parameters of an atom's relation
     * @param atom the atom to report on
     * @return number of auxiliary attributes
     */
    size_t getArity(const AstAtom* atom) const {
        return computeArity(getRelation(*program, atom->getQualifiedName()));
    }

    /**
     * Returns the number of auxiliary parameters of a relation
     * @param relation the relation to report on
     * @return number of auxiliary attributes
     */
    size_t getArity(const AstRelation* relation) const {
        return computeArity(relation);
    }

private:
    /**
     * Returns the number of auxiliary parameters of a relation
     * @param relation the relation to report on
     * @return number of auxiliary attributes
     */
    size_t computeArity(const AstRelation* relation) const;

    const AstProgram* program = nullptr;
};

}  // end of namespace souffle
