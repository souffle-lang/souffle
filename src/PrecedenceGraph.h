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

#include <iostream>
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
    /** Adjacency list of precedence graph (determined by the dependencies of the relations) */
    AstRelationGraph precedenceGraph;

public:
    static constexpr const char* name = "precedence-graph";

    void run(const AstTranslationUnit& translationUnit) override;

    /** Output precedence graph in graphviz format to a given stream */
    void outputPrecedenceGraph(std::ostream& os) const;

    const AstRelationSet& getPredecessors(const AstRelation* relation) const {
        assert(precedenceGraph.contains(relation) && "Relation not present in precedence graph!");
        return precedenceGraph.getEdges(relation);
    }

    const AstRelationSet& getSuccessors(const AstRelation* relation) const {
        assert(precedenceGraph.contains(relation) && "Relation not present in precedence graph!");
        return precedenceGraph.getReverseEdges(relation);
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

    void run(const AstTranslationUnit& translationUnit) override;

    const std::set<const AstRelation*>& getRedundantRelations() const {
        return redundantRelations;
    }
};

/**
 * Analysis pass identifying clauses which are recursive.
 */
class RecursiveClauses : public AstAnalysis {
private:
    std::set<const AstClause*> recursiveClauses;

    /** Determines whether the given clause is recursive within the given program */
    bool computeIsRecursive(const AstClause& clause, const AstTranslationUnit& translationUnit) const;

public:
    static constexpr const char* name = "recursive-clauses";

    void run(const AstTranslationUnit& translationUnit) override;

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

    /** Map from node number to SCC number */
    std::map<const AstRelation*, int> nodeToSCC;

    /** Adjacency lists for the SCC graph */
    std::vector<std::set<unsigned>> succSCC;

    /** Predecessor set for the SCC graph */
    std::vector<std::set<unsigned>> predSCC;

    /** Relations contained in a SCC */
    std::vector<std::set<const AstRelation*>> SCC;

    /** Recursive scR method for computing SCC */
    void scR(const AstRelation* relation, std::map<const AstRelation*, int>& preOrder, unsigned& counter,
            std::stack<const AstRelation*>& S, std::stack<const AstRelation*>& P, unsigned& numSCCs);

public:
    static constexpr const char* name = "scc-graph";

    void run(const AstTranslationUnit& translationUnit) override;

    unsigned getSCCForRelation(const AstRelation* relation) const {
        return nodeToSCC.at(relation);
    }

    /** True if the given relation has any successors in another SCC, false otherwise. **/
    bool isOutbound(const AstRelation* relation) const {
        const int scc = nodeToSCC.at(relation);
        for (auto succ : precedenceGraph->getSuccessors(relation))
            if (nodeToSCC.at(succ) != scc) return true;
        return false;
    }

    /** True if the given relation has any predecessors in another SCC, false otherwise. **/
    bool isInbound(const AstRelation* relation) const {
        const int scc = nodeToSCC.at(relation);
        for (auto pred : precedenceGraph->getPredecessors(relation))
            if (nodeToSCC.at(pred) != scc) return true;
        return false;
    }

    /** True if both given relations are in the same SCC, false otherwise. **/
    bool isInSameSCC(const AstRelation* relation1, const AstRelation* relation2) const {
        return nodeToSCC.at(relation1) == nodeToSCC.at(relation2);
    }

    bool isRecursive(const unsigned scc) const {
        const std::set<const AstRelation*>& sccRelations = SCC.at(scc);
        if (sccRelations.size() == 1) {
            const AstRelation* singleRelation = *sccRelations.begin();
            if (!precedenceGraph->getPredecessors(singleRelation).count(singleRelation)) {
                return false;
            }
        }
        return true;
    }

    bool isRecursive(const AstRelation* relation) const {
        return isRecursive(getSCCForRelation(relation));
    }

    /** Return the number of strongly connected components in the SCC graph */
    unsigned getNumSCCs() const {
        return succSCC.size();
    }

    /** Get all successor SCCs of a specified scc. */
    const std::set<unsigned>& getSuccessorSCCs(const unsigned scc) const {
        return succSCC.at(scc);
    }

    /** Get all predecessor SCCs of a specified scc. */
    const std::set<unsigned>& getPredecessorSCCs(const unsigned scc) const {
        return predSCC.at(scc);
    }

    const std::set<const AstRelation*> getRelationsForSCC(const unsigned scc) const {
        return SCC.at(scc);
    }

    /** Output strongly connected component graph in graphviz format */
    void outputSCCGraph(std::ostream& os) const;
};

/**
 * Analysis pass computing a topologically sorted strongly connected component (SCC) graph.
 */
class TopologicallySortedSCCGraph : public AstAnalysis {
private:
    /** The strongly connected component (SCC) graph. */
    SCCGraph* sccGraph;

    /** The final topological ordering of the SCCs. */
    std::vector<unsigned> orderedSCCs;

    /** Calculate the topological ordering cost of a permutation of as of yet unordered SCCs
    using the ordered SCCs. Returns -1 if the given vector is not a valid topological ordering. */
    unsigned topologicalOrderingCost(const std::vector<unsigned>& permutationOfSCCs) const;

    /** Recursive component for the forwards algorithm computing the topological ordering of the SCCs. */
    void computeTopologicalOrdering(const unsigned scc, std::vector<bool>& visited);

public:
    static constexpr const char* name = "topological-scc-graph";

    void run(const AstTranslationUnit& translationUnit) override;

    SCCGraph* getSCCGraph() const {
        return sccGraph;
    }

    const std::vector<unsigned>& getSCCOrder() const {
        return orderedSCCs;
    }

    /** Output topologically sorted strongly connected component graph in text format */
    void outputTopologicallySortedSCCGraph(std::ostream& os) const;
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

    /** Relations computed and expired relations at each step */
    std::vector<RelationScheduleStep> schedule;

    std::vector<std::set<const AstRelation*>> computeRelationExpirySchedule(
            const AstTranslationUnit& translationUnit);

public:
    static constexpr const char* name = "relation-schedule";

    void run(const AstTranslationUnit& translationUnit) override;

    const std::vector<RelationScheduleStep>& getSchedule() const {
        return schedule;
    }

    /** Check if a given relation is recursive. */
    bool isRecursive(const AstRelation* relation) const {
        return topsortSCCGraph->getSCCGraph()->isRecursive(relation);
    }

    /** Check if a given relation has predecessor relations in another SCC. */
    bool isInbound(const AstRelation* relation) const {
        return topsortSCCGraph->getSCCGraph()->isInbound(relation);
    }

    /** Check if a given relation has successor relations in another SCC. */
    bool isOutbound(const AstRelation* relation) const {
        return topsortSCCGraph->getSCCGraph()->isOutbound(relation);
    }

    /** Get all predecessor relations in a different SCC to the given relation. */
    const std::set<const AstRelation*> getInboundPredecessors(const AstRelation* relation) const {
        std::set<const AstRelation*> res;
        for (const auto& pred : precedenceGraph->getPredecessors(relation))
            if (!topsortSCCGraph->getSCCGraph()->isInSameSCC(pred, relation)) res.insert(pred);
        return res;
    }

    /** Get all successor relations in a different SCC to the given relation. */
    const std::set<const AstRelation*> getOutboundSuccessors(const AstRelation* relation) const {
        std::set<const AstRelation*> res;
        for (const auto& succ : precedenceGraph->getSuccessors(relation))
            if (!topsortSCCGraph->getSCCGraph()->isInSameSCC(succ, relation)) res.insert(succ);
        return res;
    }

    /** Get the dependency graph of the relation schedule as an adjacency list. Note that this is identical to
     * the SCC graph however with relation schedule step names represented by their index in the computed
     * topological order. */
    std::unique_ptr<std::vector<std::vector<unsigned>>> asAdjacencyList() const {
        std::unordered_map<unsigned, unsigned> sccIndices;
        unsigned i = 0;
        for (unsigned scc : topsortSCCGraph->getSCCOrder()) {
            sccIndices.insert(std::make_pair(scc, i));
            ++i;
        }

        std::unique_ptr<std::vector<std::vector<unsigned>>> adjacencyList =
                std::unique_ptr<std::vector<std::vector<unsigned>>>(new std::vector<std::vector<unsigned>>());
        for (const unsigned scc : topsortSCCGraph->getSCCOrder()) {
            std::vector<unsigned> current = std::vector<unsigned>();
            for (const unsigned succ : topsortSCCGraph->getSCCGraph()->getSuccessorSCCs(scc)) {
                current.push_back(sccIndices.at(succ));
            }
            adjacencyList->push_back(current);
        }
        return adjacencyList;
    }

    /** Dump this relation schedule to standard error. */
    void dump() const {
        std::cerr << "begin schedule\n";
        for (const RelationScheduleStep& step : schedule) {
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
