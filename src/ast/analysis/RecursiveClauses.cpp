/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecursiveClauses.cpp
 *
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "ast/analysis/RecursiveClauses.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/analysis/RelationDetailCache.h"
#include "souffle/utility/StreamUtil.h"
#include <algorithm>
#include <set>
#include <utility>
#include <vector>

namespace souffle {
class AstRelation;

void RecursiveClausesAnalysis::run(const AstTranslationUnit& translationUnit) {
    visitDepthFirst(*translationUnit.getProgram(), [&](const AstClause& clause) {
        if (computeIsRecursive(clause, translationUnit)) {
            recursiveClauses.insert(&clause);
        }
    });
}

void RecursiveClausesAnalysis::print(std::ostream& os) const {
    os << recursiveClauses << std::endl;
}

bool RecursiveClausesAnalysis::computeIsRecursive(
        const AstClause& clause, const AstTranslationUnit& translationUnit) const {
    const auto& relationDetail = *translationUnit.getAnalysis<RelationDetailCacheAnalysis>();
    const AstProgram& program = *translationUnit.getProgram();

    // we want to reach the atom of the head through the body
    const AstRelation* trg = getHeadRelation(&clause, &program);

    std::set<const AstRelation*> reached;
    std::vector<const AstRelation*> worklist;

    // set up start list
    for (const auto* cur : getBodyLiterals<AstAtom>(clause)) {
        auto rel = relationDetail.getRelation(cur->getQualifiedName());
        if (rel == trg) {
            return true;
        }
        worklist.push_back(rel);
    }

    // process remaining elements
    while (!worklist.empty()) {
        // get next to process
        const AstRelation* cur = worklist.back();
        worklist.pop_back();

        // skip null pointers (errors in the input code)
        if (cur == nullptr) {
            continue;
        }

        // check whether this one has been checked before
        if (!reached.insert(cur).second) {
            continue;
        }

        // check all atoms in the relations
        for (const AstClause* cl : relationDetail.getClauses(cur)) {
            for (const AstAtom* at : getBodyLiterals<AstAtom>(*cl)) {
                auto rel = relationDetail.getRelation(at->getQualifiedName());
                if (rel == trg) {
                    return true;
                }
                worklist.push_back(rel);
            }
        }
    }

    // no cycles found
    return false;
}

}  // end of namespace souffle
