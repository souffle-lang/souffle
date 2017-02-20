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
 * Defines the class for precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "AstAnalysis.h"
#include "AstProgram.h"
#include "AstTranslationUnit.h"
#include "GraphUtils.h"

#include <iomanip>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace souffle {

typedef Graph<const AstRelation*, AstNameComparison> AstRelationGraph;

/**
 * Analysis pass computing the precedence graph of the relations of the datalog progam.
 */
class PrecedenceGraph : public AstAnalysis {
private:
    // Adjacency list of precedence graph (determined by the dependencies of the relations)
    AstRelationGraph precedenceGraph;

public:
    static constexpr const char* name = "precedence-graph";

    virtual void run(const AstTranslationUnit& translationUnit);

    const AstRelationSet& getPredecessors(const AstRelation* relation) {
        assert(precedenceGraph.hasVertex(relation) && "Relation not present in precedence graph!");
        return precedenceGraph.getSuccessors(relation);
    }

    const AstRelationGraph getGraph() const {
        return precedenceGraph;
    }
};

/**
 * Analysis pass identifying relations which do not contribute to the computation
 * of the output relations.
 */
class RedundantRelations : public AstAnalysis {
private:
    PrecedenceGraph* precedenceGraph;

    std::set<const AstRelation*> redundantRelations;

public:
    static constexpr const char* name = "redundant-relations";

    virtual void run(const AstTranslationUnit& translationUnit);

    const std::set<const AstRelation*>& getRedundantRelations() {
        return redundantRelations;
    }
};

/**
 * Analysis pass identifying clauses which are recursive.
 */
class RecursiveClauses : public AstAnalysis {
private:
    std::set<const AstClause*> recursiveClauses;

    // Determines whether the given clause is recursive within the given program
    bool computeIsRecursive(const AstClause& clause, const AstTranslationUnit& translationUnit) const;

public:
    static constexpr const char* name = "recursive-clauses";

    virtual void run(const AstTranslationUnit& translationUnit);

    bool isRecursive(const AstClause* clause) const {
        return recursiveClauses.count(clause);
    }
};

/**
 * Analysis pass computing the strongly connected component (SCC) graph for the datalog program.
 */
class SCCGraph : public AstAnalysis {
private:
    PrecedenceGraph* precedenceGraph;

    HyperGraph<index::SetTable, const AstRelation*> sccGraph;

public:
    const HyperGraph<index::SetTable, const AstRelation*>& getGraph() {
        return sccGraph;
    }

    static constexpr const char* name = "scc-graph";

    virtual void run(const AstTranslationUnit& translationUnit) {
        precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();
        sccGraph = GraphConvert::toAcyclicHyperGraph<index::SetTable>(precedenceGraph->getGraph());
    }

    const bool isRecursive(size_t vertex) const {
        const auto& objects = sccGraph.table().get(vertex);
        if (objects.size() == 1 && !precedenceGraph->getGraph().isRecursive(*objects.begin())) return false;
        return true;
    }

    const bool isRecursive(const AstRelation* relation) const {
        return isRecursive(sccGraph.table().getIndex(relation));
    }
};

/**
 * Analysis pass computing a topologically sorted strongly connected component (SCC) graph.
 */
class TopologicallySortedSCCGraph : public AstAnalysis {
private:
    // The strongly connected component (SCC) graph.
    SCCGraph* sccGraph;

    // The final topological ordering of the SCCs.
    std::vector<size_t> orderedSCCs;

public:
    static constexpr const char* name = "topological-scc-graph";

    virtual void run(const AstTranslationUnit& translationUnit) {
        sccGraph = translationUnit.getAnalysis<SCCGraph>();
        HyperGraph<index::SeqTable, size_t> graph =
                GraphConvert::toHyperGraph<index::SeqTable>(sccGraph->getGraph());
        // TODO: find a better topological ordering algorithm
        orderedSCCs = GraphOrder::innerOrder(graph, &GraphSearch::khansAlgorithm);
    }

    SCCGraph* getSCCGraph() const {
        return sccGraph;
    }

    const std::vector<size_t>& getSCCOrder() const {
        return orderedSCCs;
    }

    void outputTopologicallySortedSCCGraph(std::ostream& os) const {
        for (size_t i = 0; i < orderedSCCs.size(); i++)
            os << "[" << join(sccGraph->getGraph().table().get(orderedSCCs[i])) << "]\n";
        os << "\n";
        os << "cost: " << GraphQuery::topologicalOrderingCost(sccGraph->getGraph(), orderedSCCs) << "\n";
    }
};

/**
 * A single step in a relation schedule, consisting of the relations computed in the step
 * and the relations that are no longer required at that step.
 */
class RelationScheduleStep {
private:
    std::set<const AstRelation*> computedRelations;
    std::set<const AstRelation*> expiredRelations;
    const bool recursive;

public:
    RelationScheduleStep(std::set<const AstRelation*> computedRelations,
            std::set<const AstRelation*> expiredRelations, const bool recursive)
            : computedRelations(computedRelations), expiredRelations(expiredRelations), recursive(recursive) {
    }

    const std::set<const AstRelation*>& getComputedRelations() const {
        return computedRelations;
    }

    const std::set<const AstRelation*>& getExpiredRelations() const {
        return expiredRelations;
    }

    bool isRecursive() const {
        return recursive;
    }
};

/**
 * Analysis pass computing a schedule for computing relations.
 */
class RelationSchedule : public AstAnalysis {
private:
    TopologicallySortedSCCGraph* topsortSCCGraph;
    PrecedenceGraph* precedenceGraph;

    // Relations computed and expired relations at each step
    std::vector<RelationScheduleStep> schedule;

    std::vector<std::set<const AstRelation*>> computeRelationExpirySchedule(
            const AstTranslationUnit& translationUnit);

public:
    static constexpr const char* name = "relation-schedule";

    virtual void run(const AstTranslationUnit& translationUnit);

    const std::vector<RelationScheduleStep>& getSchedule() {
        return schedule;
    }

    bool isRecursive(const AstRelation* relation) {
        return topsortSCCGraph->getSCCGraph()->isRecursive(relation);
    }

    void dump() {
        std::cerr << "begin schedule\n";
        for (RelationScheduleStep& step : schedule) {
            std::cerr << "computed: ";
            for (const AstRelation* compRel : step.getComputedRelations()) {
                std::cerr << compRel->getName() << ", ";
            }
            std::cerr << "\nexpired: ";
            for (const AstRelation* compRel : step.getExpiredRelations()) {
                std::cerr << compRel->getName() << ", ";
            }
            std::cerr << "\n";
            if (step.isRecursive()) {
                std::cerr << "recursive";
            } else {
                std::cerr << "not recursive";
            }
            std::cerr << "\n";
        }
        std::cerr << "end schedule\n";
    }
};

}  // end of namespace souffle
