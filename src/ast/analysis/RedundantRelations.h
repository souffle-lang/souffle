/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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

#include "ast/analysis/Analysis.h"
#include <iostream>
#include <set>
#include <string>

namespace souffle {
class AstTranslationUnit;
class PrecedenceGraphAnalysis;
class AstRelation;

/**
 * Analysis pass identifying relations which do not contribute to the computation
 * of the output relations.
 */
class RedundantRelationsAnalysis : public AstAnalysis {
public:
    static constexpr const char* name = "redundant-relations";

    RedundantRelationsAnalysis() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const std::set<const AstRelation*>& getRedundantRelations() const {
        return redundantRelations;
    }

private:
    PrecedenceGraphAnalysis* precedenceGraph = nullptr;

    std::set<const AstRelation*> redundantRelations;
};

}  // end of namespace souffle
