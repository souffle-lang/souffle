/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PrecedenceGraph.cpp
 *
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "ast/analysis/PrecedenceGraph.h"
#include "GraphUtils.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Visitor.h"
#include <set>
#include <string>
#include <vector>

namespace souffle::ast::analysis {

void PrecedenceGraphAnalysis::run(const TranslationUnit& translationUnit) {
    /* Get relations */
    Program& program = translationUnit.getProgram();

    for (const auto* r : program.getRelations()) {
        backingGraph.insert(r);

        auto addEdgeToR = [&](const Atom& atom) { backingGraph.insert(program.getRelation(atom), r); };
        for (auto&& c : program.getClauses(*r)) {
            souffle::visit(c->getHead()->getArguments(), addEdgeToR);
            auto literals = c->getBodyLiterals();
            for (std::size_t i = as<SubsumptiveClause>(c) ? 2 : 0; i < literals.size(); i++) {
                souffle::visit(literals[i], addEdgeToR);
            }
        }

        // delta_debug relation are computed from the original relation
        if (const auto deltaDebug = r->getIsDeltaDebug()) {
            const auto* dbg = program.getRelation(deltaDebug.value());
            backingGraph.insert(r, dbg);
            backingGraph.insert(dbg, r);
        }
    }
}

void PrecedenceGraphAnalysis::printRaw(std::stringstream& ss) const {
    /* Print dependency graph */
    ss << "digraph {\n";
    /* Print node of dependence graph */
    for (const Relation* rel : backingGraph.vertices()) {
        if (rel != nullptr) {
            ss << "\t\"" << rel->getQualifiedName() << "\" [label = \"" << rel->getQualifiedName()
               << "\"];\n";
        }
    }
    for (const Relation* rel : backingGraph.vertices()) {
        if (rel != nullptr) {
            for (const Relation* adjRel : backingGraph.successors(rel)) {
                if (adjRel != nullptr) {
                    ss << "\t\"" << rel->getQualifiedName() << "\" -> \"" << adjRel->getQualifiedName()
                       << "\";\n";
                }
            }
        }
    }
    ss << "}\n";
}

void PrecedenceGraphAnalysis::print(std::ostream& os) const {
    std::stringstream ss;
    printRaw(ss);
    os << ss.str();
}

void PrecedenceGraphAnalysis::printHTML(std::ostream& os) const {
    std::stringstream ss;
    printRaw(ss);
    printHTMLGraph(os, ss.str(), getName());
}

}  // namespace souffle::ast::analysis
