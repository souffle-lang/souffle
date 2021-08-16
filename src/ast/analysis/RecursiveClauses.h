/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecursiveClauses.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include <iostream>
#include <set>
#include <string>

namespace souffle::ast {

class Clause;
class TranslationUnit;

namespace analysis {

/**
 * Analysis pass identifying clauses which are recursive.
 */
class RecursiveClausesAnalysis : public Analysis {
public:
    static constexpr const char* name = "recursive-clauses";

    RecursiveClausesAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    bool recursive(const Clause* clause) const {
        return recursiveClauses.count(clause) != 0u;
    }

private:
    std::set<const Clause*> recursiveClauses;

    /** Determines whether the given clause is recursive within the given program */
    bool computeIsRecursive(const Clause& clause, const TranslationUnit& translationUnit) const;
};

}  // namespace analysis
}  // namespace souffle::ast
