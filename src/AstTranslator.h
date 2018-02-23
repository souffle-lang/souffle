/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTranslator.h
 *
 * Defines utilities for translating AST structures into RAM constructs.
 *
 ***********************************************************************/

#pragma once

#include "AstTranslationUnit.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "RamTranslationUnit.h"

#include <map>
#include <vector>

namespace souffle {

// forward declarations
class AstRelation;
class AstAtom;
class AstClause;
class AstProgram;

class RamStatement;

class RecursiveClauses;

/**
 * A utility class capable of conducting the conversion between AST
 * and RAM structures.
 */
class AstTranslator {
    /** If true, created constructs will be annotated with logging information */

public:
    /**
     * A constructor for this translators.
     *
     * @param logging if generated clauses should be annotated with logging operations
     */

    /**
     * Converts the given relation identifier into a relation name.
     */
    std::string translateRelationName(const AstRelationIdentifier& id);

    /** generate RAM code for a clause */
    std::unique_ptr<RamStatement> translateClause(const AstClause& clause, const AstProgram* program,
            const TypeEnvironment* typeEnv, int version = 0, bool ret = false, bool hashset = false);

    /**
     * Generates RAM code for the non-recursive clauses of the given relation.
     *
     * @return a corresponding statement or null if there are no non-recursive clauses.
     */
    std::unique_ptr<RamStatement> translateNonRecursiveRelation(const AstRelation& rel,
            const AstProgram* program, const RecursiveClauses* recursiveClauses,
            const TypeEnvironment& typeEnv);

    /** generate RAM code for recursive relations in a strongly-connected component */
    std::unique_ptr<RamStatement> translateRecursiveRelation(const std::set<const AstRelation*>& scc,
            const AstProgram* program, const RecursiveClauses* recursiveClauses,
            const TypeEnvironment& typeEnv);

    /** generate RAM code for subroutine to get subproofs */
    std::unique_ptr<RamStatement> makeSubproofSubroutine(
            const AstClause& clause, const AstProgram* program, const TypeEnvironment& typeEnv);

    /** Translate AST to RamProgram */
    std::unique_ptr<RamProgram> translateProgram(const AstTranslationUnit& translationUnit);

    /** translates AST to translation unit  */
    std::unique_ptr<RamTranslationUnit> translateUnit(AstTranslationUnit& tu) {
        std::unique_ptr<RamProgram> ramProg = translateProgram(tu);
        SymbolTable& symTab = tu.getSymbolTable();
        ErrorReport& errReport = tu.getErrorReport();
        DebugReport& debugReport = tu.getDebugReport();
        return std::make_unique<RamTranslationUnit>(std::move(ramProg), symTab, errReport, debugReport);
    }
};

}  // end of namespace souffle
