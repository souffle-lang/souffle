/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RedundantRelations.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include <set>
#include <string>

namespace souffle::ast {
class TranslationUnit;
class Relation;

namespace analysis {
class PrecedenceGraphAnalysis;

/**
 * Analysis pass identifying relations which do not contribute to the computation
 * of the output relations.
 */
class RedundantRelationsAnalysis : public Analysis {
public:
    static constexpr const char* name = "redundant-relations";

    RedundantRelationsAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const std::set<QualifiedName>& getRedundantRelations() const {
        return redundantRelations;
    }

private:
    PrecedenceGraphAnalysis* precedenceGraph = nullptr;
    std::set<QualifiedName> redundantRelations;
};

}  // namespace analysis
}  // namespace souffle::ast
