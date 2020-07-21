/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstGroundAnalysis.h
 *
 * Defines a function for computing the grounded arguments in a clause
 *
 ***********************************************************************/

#pragma once

#include <map>

namespace souffle {

class AstArgument;
class AstClause;
class AstTranslationUnit;

/**
 * Analyse the given clause and computes for each contained argument
 * whether it is a grounded value or not.
 *
 * @param tu the translation unit containing the clause
 * @param clause the clause to be analyzed
 * @return a map mapping each contained argument to a boolean indicating
 *      whether the argument represents a grounded value or not
 */
std::map<const AstArgument*, bool> getGroundedTerms(const AstTranslationUnit& tu, const AstClause& clause);

}  // end of namespace souffle
