/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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

#include "ast/analysis/Analysis.h"
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstTranslationUnit;
class PrecedenceGraphAnalysis;
class TopologicallySortedSCCGraphAnalysis;
class AstRelation;

/**
 * A single step in a relation schedule, consisting of the relations computed in the step
 * and the relations that are no longer required at that step.
 */
class RelationScheduleAnalysisStep {
public:
    RelationScheduleAnalysisStep(std::set<const AstRelation*> computedRelations,
            std::set<const AstRelation*> expiredRelations, const bool isRecursive)
            : computedRelations(std::move(computedRelations)), expiredRelations(std::move(expiredRelations)),
              isRecursive(isRecursive) {}

    const std::set<const AstRelation*>& computed() const {
        return computedRelations;
    }

    const std::set<const AstRelation*>& expired() const {
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
    std::set<const AstRelation*> computedRelations;
    std::set<const AstRelation*> expiredRelations;
    const bool isRecursive;
};

/**
 * Analysis pass computing a schedule for computing relations.
 */
class RelationScheduleAnalysis : public AstAnalysis {
public:
    static constexpr const char* name = "relation-schedule";

    RelationScheduleAnalysis() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

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

    std::vector<std::set<const AstRelation*>> computeRelationExpirySchedule(
            const AstTranslationUnit& translationUnit);
};

}  // end of namespace souffle
