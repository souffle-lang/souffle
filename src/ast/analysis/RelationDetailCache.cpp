/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationDetailCache.cpp
 *
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "ast/analysis/RelationDetailCache.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include <set>
#include <utility>
#include <vector>

namespace souffle {

void RelationDetailCacheAnalysis::run(const AstTranslationUnit& translationUnit) {
    const auto& program = *translationUnit.getProgram();
    for (auto* rel : program.getRelations()) {
        nameToRelation[rel->getQualifiedName()] = rel;
        nameToClauses[rel->getQualifiedName()] = std::set<AstClause*>();
    }
    for (auto* clause : program.getClauses()) {
        const auto& relationName = clause->getHead()->getQualifiedName();
        if (nameToClauses.find(relationName) == nameToClauses.end()) {
            nameToClauses[relationName] = std::set<AstClause*>();
        }
        nameToClauses.at(relationName).insert(clause);
    }
}

void RelationDetailCacheAnalysis::print(std::ostream& os) const {
    for (const auto& pair : nameToClauses) {
        os << "--" << pair.first << "--";
        os << std::endl;
        for (const auto* clause : pair.second) {
            os << *clause << std::endl;
        }
        os << std::endl;
    }
}

}  // end of namespace souffle
