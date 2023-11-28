/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PrecedenceGraph.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "GraphUtils.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include <string>

namespace souffle::ast {

class TranslationUnit;

namespace analysis {

/**
 * Analysis pass computing the precedence graph of the relations of the datalog progam.
 */
class PrecedenceGraphAnalysis : public Analysis {
public:
    static constexpr const char* name = "precedence-graph";

    PrecedenceGraphAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    /** Output precedence graph in text format to a given stream */
    void print(std::ostream& os) const override;

    /** Output precedence graph in image format to a given stream */
    void printHTML(std::ostream& os) const override;

    const Graph<const Relation*, NameComparison>& graph() const {
        return backingGraph;
    }

private:
    /** Adjacency list of precedence graph (determined by the dependencies of the relations) */
    Graph<const Relation*, NameComparison> backingGraph;

    /** Output precedence graph in text format to a given stringstream */
    void printRaw(std::stringstream& ss) const;
};

}  // namespace analysis
}  // namespace souffle::ast
