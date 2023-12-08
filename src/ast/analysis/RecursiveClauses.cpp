/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecursiveClauses.cpp
 *
 * Compute the set of recursive clauses.
 *
 * A recursive clause is a clause of a rule R that depends directly or
 * transitively on that rule R.
 *
 ***********************************************************************/

#include "ast/analysis/RecursiveClauses.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/StreamUtil.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

void RecursiveClausesAnalysis::run(const TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    // Mapping from a relation to the set of relations that it depends on directly.
    //
    // It is the adjacency list of the relations dependency graph.
    std::map<uint32_t, std::set<uint32_t>> relationUseRelation;

    // Mapping from a clause to the set of relations that it depends on directly.
    std::map<const Clause*, std::set<uint32_t>> clauseUseRelation;

    // Mapping from a clause to its relations.
    std::map<const Clause*, uint32_t> clauseRelation;

    std::vector<uint32_t> relations;

    // gather dependencies
    for (const auto& qninfo : program.getRelationInfo()) {
        const uint32_t head = qninfo.first.getIndex();
        relations.emplace_back(head);
        for (const auto& clause : qninfo.second.clauses) {
            clauseRelation.emplace(clause.get(), head);
            for (const auto& atom : getBodyLiterals<Atom>(*clause)) {
                const uint32_t rhs = atom->getQualifiedName().getIndex();
                relationUseRelation[head].emplace(rhs);
                clauseUseRelation[clause.get()].emplace(rhs);
            }
        }
    }

    // Mapping from a relation to the set of transitively reachable relations
    // it depends on, including itself.
    std::map<uint32_t, std::set<uint32_t>> reachableRelation;

    // called when we discoved that `head` transitively reach `reached`.
    const std::function<void(uint32_t, uint32_t)> dfs = [&](uint32_t head, uint32_t reached) {
        reachableRelation[head].emplace(reached);
        for (uint32_t rel : relationUseRelation[reached]) {
            if (reachableRelation[head].emplace(rel).second) {
                // discovered that relation `rel` is reachabel from `head`
                dfs(head, rel);
            }
        }
    };

    // Compute the transitive closure of reachable (dependencies) relations from each relation.
    for (const uint32_t head : relations) {
        // include itself in the closure
        dfs(head, head);
    }

    for (const auto& [clause, rel] : clauseRelation) {
        for (const uint32_t used : clauseUseRelation[clause]) {
            if (reachableRelation[used].count(rel) > 0) {
                // clause is recursive
                recursiveClauses.emplace(clause);
                break;
            }
        }
    }
}

void RecursiveClausesAnalysis::print(std::ostream& os) const {
    os << recursiveClauses << std::endl;
}

}  // namespace souffle::ast::analysis
