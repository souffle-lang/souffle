/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationSchedule.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "ast/analysis/TopologicallySortedSCCGraph.h"
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

/**
 * A single step in a relation schedule, consisting of the relations computed in the step
 * and the relations that are no longer required at that step.
 */
class RelationScheduleAnalysisStep {
public:
    RelationScheduleAnalysisStep(std::set<const Relation*> computedRelations,
            std::set<const Relation*> expiredRelations, const bool isRecursive)
            : computedRelations(std::move(computedRelations)), expiredRelations(std::move(expiredRelations)),
              isRecursive(isRecursive) {}

    const std::set<const Relation*>& computed() const {
        return computedRelations;
    }

    const std::set<const Relation*>& expired() const {
        return expiredRelations;
    }

    bool recursive() const {
        return isRecursive;
    }

    void print(std::ostream& os) const;

    /** Add support for printing nodes */
    friend std::ostream& operator<<(std::ostream& out, const RelationScheduleAnalysisStep& other) {
        other.print(out);
        return out;
    }

private:
    std::set<const Relation*> computedRelations;
    std::set<const Relation*> expiredRelations;
    const bool isRecursive;
};

/**
 * Analysis pass computing a schedule for computing relations.
 */
class RelationScheduleAnalysis : public Analysis {
public:
    static constexpr const char* name = "relation-schedule";

    RelationScheduleAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    const std::vector<RelationScheduleAnalysisStep>& schedule() const {
        return relationSchedule;
    }

    /** Dump this relation schedule to standard error. */
    void print(std::ostream& os) const override;

private:
    TopologicallySortedSCCGraphAnalysis* topsortSCCGraphAnalysis = nullptr;
    PrecedenceGraphAnalysis* precedenceGraph = nullptr;

    /** Relations computed and expired relations at each step */
    std::vector<RelationScheduleAnalysisStep> relationSchedule;

    std::vector<std::set<const Relation*>> computeRelationExpirySchedule(
            const TranslationUnit& translationUnit);
};

}  // namespace souffle::ast::analysis
