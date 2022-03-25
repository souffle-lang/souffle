/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderLiterals.cpp
 *
 * Define classes and functionality related to the ReorderLiterals
 * transformer.
 *
 ***********************************************************************/

#include "ast/transform/ReorderLiterals.h"
#include "Global.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Program.h"
#include "ast/SubsumptiveClause.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/ProfileUse.h"
#include "ast/utility/BindingStore.h"
#include "ast/utility/SipsMetric.h"
#include "ast/utility/Utils.h"
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

Clause* ReorderLiteralsTransformer::reorderClauseWithSips(const SipsMetric& sips, const Clause* clause) {
    // ignore clauses with fixed execution plans or a clause is a subsumptive clause
    if (clause->getExecutionPlan() != nullptr || isA<SubsumptiveClause>(clause)) {
        return nullptr;
    }

    // get the ordering corresponding to the SIPS
    std::vector<std::size_t> newOrdering = sips.getReordering(clause);

    // check if we need a change
    bool changeNeeded = false;
    for (std::size_t i = 0; i < newOrdering.size(); i++) {
        if (newOrdering[i] != i) {
            changeNeeded = true;
        }
    }

    // reorder if needed
    return changeNeeded ? reorderAtoms(clause, newOrdering) : nullptr;
}

bool ReorderLiteralsTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    // --- SIPS-based static reordering ---
    // ordering is based on the given SIPS

    // default SIPS to choose is 'all-bound'
    std::string sipsChosen = "all-bound";
    if (Global::config().has("SIPS")) {
        sipsChosen = Global::config().get("SIPS");
    }
    auto sipsFunction = SipsMetric::create(sipsChosen, translationUnit);

    // literal reordering is a rule-local transformation
    std::vector<Clause*> clausesToRemove;

    for (Clause* clause : program.getClauses()) {
        Clause* newClause = reorderClauseWithSips(*sipsFunction, clause);
        if (newClause != nullptr) {
            // reordering needed - swap around
            clausesToRemove.push_back(clause);
            program.addClause(Own<Clause>(newClause));
        }
    }

    changed |= !clausesToRemove.empty();
    program.removeClauses(clausesToRemove);

    // --- profile-guided reordering ---
    if (Global::config().has("profile-use")) {
        // parse supplied profile information
        auto profilerSips = SipsMetric::create("profiler", translationUnit);

        // change the ordering of literals within clauses
        std::vector<Clause*> clausesToRemove;
        for (Clause* clause : program.getClauses()) {
            Clause* newClause = reorderClauseWithSips(*profilerSips, clause);
            if (newClause != nullptr) {
                // reordering needed - swap around
                clausesToRemove.push_back(clause);
                program.addClause(Own<Clause>(newClause));
            }
        }

        changed |= !clausesToRemove.empty();
        program.removeClauses(clausesToRemove);
    }

    return changed;
}

}  // namespace souffle::ast::transform
