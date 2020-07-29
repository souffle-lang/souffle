/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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

#include "ast/analysis/Analysis.h"
#include <iostream>
#include <set>
#include <string>

namespace souffle {

class AstClause;
class AstTranslationUnit;

/**
 * Analysis pass identifying clauses which are recursive.
 */
class RecursiveClausesAnalysis : public AstAnalysis {
public:
    static constexpr const char* name = "recursive-clauses";

    RecursiveClausesAnalysis() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    bool recursive(const AstClause* clause) const {
        return recursiveClauses.count(clause) != 0u;
    }

private:
    std::set<const AstClause*> recursiveClauses;

    /** Determines whether the given clause is recursive within the given program */
    bool computeIsRecursive(const AstClause& clause, const AstTranslationUnit& translationUnit) const;
};

}  // end of namespace souffle
