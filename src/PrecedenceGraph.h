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

#include <map>
#include <vector>
#include <string>
#include <stack>
#include <list>
#include <iomanip>

#include "AstProgram.h"
#include "AstAnalysis.h"
#include "AstTranslationUnit.h"
#include "GraphUtils.h"
#include "Environment.h"

namespace souffle {

/**
 * Analysis pass computing the precedence graph of the relations of the datalog progam.
 */
class PrecedenceGraph : public AstAnalysis {
private:
    /** Adjacency list of precedence graph (determined by the dependencies of the relations) */
    Graph<const AstRelation *> precedenceGraph;

public:
    static constexpr const char *name = "precedence-graph";

    virtual void run(const AstTranslationUnit &translationUnit);

    /** Output precedence graph in graphviz format to a given stream */
    void outputPrecedenceGraph(std::ostream &os);

    const std::set<const AstRelation *> &getPredecessors(const AstRelation *relation) {
        assert(precedenceGraph.contains(relation) && "Relation not present in precedence graph!");
        return precedenceGraph.getEdges(relation);
    }

    const Graph<const AstRelation *> getGraph() const {
        return precedenceGraph;
    }
};

/**
 * Analysis pass identifying relations which do not contribute to the computation
 * of the output relations.
 */
class RedundantRelations : public AstAnalysis {
private:
    PrecedenceGraph *precedenceGraph;

    std::set<const AstRelation *> redundantRelations;

public:
    static constexpr const char *name = "redundant-relations";

    virtual void run(const AstTranslationUnit &translationUnit);

    const std::set<const AstRelation *> &getRedundantRelations() {
        return redundantRelations;
    }

};

/**
 * Analysis pass identifying clauses which are recursive.
 */
class RecursiveClauses : public AstAnalysis {
private:
    std::set<const AstClause *> recursiveClauses;

    /** Determines whether the given clause is recursive within the given program */
    bool computeIsRecursive(const AstClause& clause, const AstTranslationUnit& translationUnit) const;
public:
    static constexpr const char *name = "recursive-clauses";

    virtual void run(const AstTranslationUnit &translationUnit);

    bool isRecursive(const AstClause *clause) const {
        return recursiveClauses.count(clause);
    }
};

/**
 * Analysis pass computing the strongly connected component (SCC) graph for the datalog program.
 */
class SCCGraph : public AstAnalysis {
private:
    PrecedenceGraph *precedenceGraph;

    /** Map from node number to SCC number */
    std::map<const AstRelation *, int> nodeToSCC;

    /** List of colors of SCC nodes, default is black. */
    std::vector <unsigned int> sccColor;

    /** Adjacency lists for the SCC graph */
    std::vector <std::set<int> > succSCC;

    /** Predecessor set for the SCC graph */
    std::vector <std::set<int> > predSCC;

    /** Relations contained in a SCC */
    std::vector<std::set<const AstRelation *>> SCC;

    /** Recursive scR method for computing SCC */
    void scR(const AstRelation *relation, std::map<const AstRelation *, int> &preOrder, unsigned int &counter,
            std::stack<const AstRelation *> &S, std::stack<const AstRelation *> &P, int &numSCCs);

public:

    static constexpr const char *name = "scc-graph";

    virtual void run(const AstTranslationUnit &translationUnit);

    int getSCCForRelation(const AstRelation *relation) {
        return nodeToSCC[relation];
    }

    bool isRecursive(int scc) {
        const std::set<const AstRelation *> &sccRelations = SCC[scc];
        if (sccRelations.size() == 1) {
            const AstRelation *singleRelation = *sccRelations.begin();
            if (!precedenceGraph->getPredecessors(singleRelation).count(singleRelation)) {
                return false;
            }
        }
        return true;
    }

    bool isRecursive(const AstRelation *relation) {
        return isRecursive(getSCCForRelation(relation));
    }

    /** Return the number of strongly connected components in the SCC graph */
    int getNumSCCs() {
        return succSCC.size();
    }

    /** Get the color of an SCC. */
    const unsigned int getColor(const int scc) {
        return sccColor[scc];
    }

    /** Set the color of an SCC. */
    void setColor(const int scc, const unsigned int color) {
        sccColor[scc] = color;
    }

    /** Fill all SCCs to the given color. */
    void fillColors(const unsigned int color) {
        std::fill(sccColor.begin(), sccColor.end(), color);
    }

    /** Check if a given SCC has a predecessor of the specified color. */
    const bool hasPredecessorOfColor(int scc, const unsigned int color) {
        for (auto pred : getPredecessorSCCs(scc)) if (getColor(pred) == color) return true;
        return false;
    }

    /** Check if a given SCC has a successor of the specified color. */
    const bool hasSuccessorOfColor(int scc, const unsigned int color) {
        for (auto succ : getSuccessorSCCs(scc)) if (getColor(succ) == color) return true;
        return false;
    }

    /** Get all successor SCCs of a specified scc. */
    const std::set<int> &getSuccessorSCCs(int scc) {
        return succSCC[scc];
    }

    /** Get all predecessor SCCs of a specified scc. */
    const std::set<int> &getPredecessorSCCs(int scc) {
        return predSCC[scc];
    }

    const std::set<const AstRelation *> getRelationsForSCC(int scc) {
        return SCC[scc];
    }

    /** Output strongly connected component graph in graphviz format */
    void outputSCCGraph(std::ostream &os);

};

/**
 * Analysis pass computing a topologically sorted strongly connected component (SCC) graph.
 */
class TopologicallySortedSCCGraph : public AstAnalysis {

private:

    /** The strongly connected component (SCC) graph. */
    SCCGraph *sccGraph;

    /** The final topological ordering of the SCCs. */
    std::vector<int> orderedSCCs;

    /** Marker type to compute topological ordering. */
    enum Colour {
        WHITE   = 0xFFFFFF,
        GRAY    = 0x7f7f7f,
        BLACK   = 0x000000,
        RED     = 0xFF0000
    };

    /** Calculate the topological ordering cost of a permutation of as of yet unordered SCCs
    using the ordered SCCs. Returns -1 if the given vector is not a valid topological ordering. */
    const int topologicalOrderingCost(const std::vector<int>& permutationOfSCCs) const;

    /** Compute the best cost topological ordering of the as of yet unordered SCCs in the lookahead
    set using the ordered SCCs. */
    void bestCostTopologicalOrdering(std::vector<int>& lookaheadSCCs) const;

    /** Recursive component for the backwards algorithm computing the topological ordering of the SCCs. */
    void backwardAlgorithmRecursive(int su, std::vector<int>& lookaheadSCCs);

    /** Backwards algorithm for computing the topological order of SCCs, based on reverse DFS. */
    void backwardAlgorithm();

    /** Traverse the graph and construct the set of lookahead SCCs for the forwards algorithm. */
    void findForwardLookahead(int scc, std::vector<int>& lookaheadSCCs, unsigned int depth);

    /** Recursive component for the forwards algorithm computing the topological ordering of the SCCs. */
    void forwardAlgorithmRecursive(int scc);

    /* Forwardss algorithm for computing the topological order of SCCs, based on Khan's algorithm. */
    void forwardAlgorithm();

public:

    /** Breadth limit for algorithm, used for the forwards algorithm. */
    static unsigned int BREADTH_LIMIT;

    /** Depth limit for algorithm, used for the forwards algorithm. */
    static unsigned int DEPTH_LIMIT;

    /** Lookahead limit for algorithm, used for the backwards algorithm. */
    static unsigned int LOOKAHEAD;

    static constexpr const char *name = "topological-scc-graph";

    virtual void run(const AstTranslationUnit &translationUnit);

    SCCGraph *getSCCGraph() {
        return sccGraph;
    }

    const std::vector<int> &getSCCOrder() {
        return orderedSCCs;
    }

    /** Output topologically sorted strongly connected component graph in text format */
    void outputTopologicallySortedSCCGraph(std::ostream &os);

};

/**
 * A single step in a relation schedule, consisting of the relations computed in the step
 * and the relations that are no longer required at that step.
 */
class RelationScheduleStep {
private:
    std::set<const AstRelation *> computedRelations;
    std::set<const AstRelation *> expiredRelations;
    const bool recursive;
public:
    RelationScheduleStep(std::set<const AstRelation *> computedRelations, std::set<const AstRelation *> expiredRelations,
            const bool recursive) : computedRelations(computedRelations), expiredRelations(expiredRelations),
            recursive(recursive) { }

    const std::set<const AstRelation *> &getComputedRelations() const {
        return computedRelations;
    }

    const std::set<const AstRelation *> &getExpiredRelations() const {
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
    TopologicallySortedSCCGraph *topsortSCCGraph;
    PrecedenceGraph *precedenceGraph;

    /** Relations computed and expired relations at each step */
    std::vector<RelationScheduleStep> schedule;

    std::vector<std::set<const AstRelation *>> computeRelationExpirySchedule(const AstTranslationUnit &translationUnit);

public:
    static constexpr const char *name = "relation-schedule";

    virtual void run(const AstTranslationUnit &translationUnit);

    const std::vector<RelationScheduleStep> &getSchedule() {
        return schedule;
    }

    bool isRecursive(const AstRelation *relation) {
        return topsortSCCGraph->getSCCGraph()->isRecursive(relation);
    }

    void dump() {
        std::cerr << "begin schedule\n";
        for (RelationScheduleStep &step : schedule) {
            std::cerr << "computed: ";
            for (const AstRelation *compRel : step.getComputedRelations()) {
                std::cerr << compRel->getName() << ", ";
            }
            std::cerr << "\nexpired: ";
            for (const AstRelation *compRel : step.getExpiredRelations()) {
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

} // end of namespace souffle

