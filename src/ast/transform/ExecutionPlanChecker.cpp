/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExecutionPlanChecker.cpp
 *
 * Implementation of the execution plan checker pass.
 *
 ***********************************************************************/

#include "ast/transform/ExecutionPlanChecker.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/ExecutionOrder.h"
#include "ast/ExecutionPlan.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/SubsumptiveClause.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/RecursiveClauses.h"
#include "ast/analysis/RelationSchedule.h"
#include "ast/utility/Utils.h"
#include "reports/ErrorReport.h"
#include "souffle/utility/ContainerUtil.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool ExecutionPlanChecker::transform(TranslationUnit& translationUnit) {
    auto& relationSchedule = translationUnit.getAnalysis<analysis::RelationScheduleAnalysis>();
    auto& recursiveClauses = translationUnit.getAnalysis<analysis::RecursiveClausesAnalysis>();
    auto&& report = translationUnit.getErrorReport();

    Program& program = translationUnit.getProgram();
    for (const analysis::RelationScheduleAnalysisStep& step : relationSchedule.schedule()) {
        const std::set<const Relation*>& scc = step.computed();
        for (const Relation* rel : scc) {
            for (auto&& clause : program.getClauses(*rel)) {
                if (!recursiveClauses.recursive(clause)) {
                    continue;
                }
                if (clause->getExecutionPlan() == nullptr) {
                    continue;
                }
                if (isA<SubsumptiveClause>(clause)) {
                    continue;
                }
                std::size_t version = 0;
                for (const auto* atom : getBodyLiterals<Atom>(*clause)) {
                    if (scc.count(program.getRelation(*atom)) != 0u) {
                        version++;
                    }
                }
                std::optional<std::size_t> maxVersion;
                for (auto const& cur : clause->getExecutionPlan()->getOrders()) {
                    maxVersion = std::max(cur.first, maxVersion.value_or(cur.first));

                    bool isComplete = true;
                    auto order = cur.second->getOrder();
                    for (unsigned i = 1; i <= order.size(); i++) {
                        if (!contains(order, i)) {
                            isComplete = false;
                            break;
                        }
                    }
                    auto numAtoms = getBodyLiterals<Atom>(*clause).size();
                    if (order.size() != numAtoms || !isComplete) {
                        report.addError("Invalid execution order in plan", cur.second->getSrcLoc());
                    }
                }

                if (version <= *maxVersion) {
                    for (const auto& cur : clause->getExecutionPlan()->getOrders()) {
                        if (cur.first >= version) {
                            report.addDiagnostic(Diagnostic(Diagnostic::Type::ERROR,
                                    DiagnosticMessage(
                                            "execution plan for version " + std::to_string(cur.first),
                                            cur.second->getSrcLoc()),
                                    {DiagnosticMessage("only versions 0.." + std::to_string(version - 1) +
                                                       " permitted")}));
                        }
                    }
                }
            }
        }
    }
    return false;
}

}  // namespace souffle::ast::transform
