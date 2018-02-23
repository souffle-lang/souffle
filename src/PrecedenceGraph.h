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

/**
 * Analysis pass computing the precedence graph of the relations of the datalog progam.
 */
class PrecedenceGraph : public AstAnalysis {
private:
    /** Adjacency list of precedence graph (determined by the dependencies of the relations) */
    Graph<const AstRelation*, AstNameComparison> backingGraph;

public:
    static constexpr const char* name = "precedence-graph";

    void run(const AstTranslationUnit& translationUnit) override;

    /** Output precedence graph in graphviz format to a given stream */
    void print(std::ostream& os) const override;

    const Graph<const AstRelation*, AstNameComparison>& graph() const {
        return backingGraph;
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

    void print(std::ostream& os) const override;

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

    void print(std::ostream& os) const override;

    bool recursive(const AstClause* clause) const {
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
    std::map<const AstRelation*, int> relationToScc;

    /** Adjacency lists for the SCC graph */
    std::vector<std::set<unsigned>> successors;

    /** Predecessor set for the SCC graph */
    std::vector<std::set<unsigned>> predecessors;

    /** Relations contained in a SCC */
    std::vector<std::set<const AstRelation*>> sccToRelation;

    /** Recursive scR method for computing SCC */
    void scR(const AstRelation* relation, std::map<const AstRelation*, int>& preOrder, unsigned& counter,
            std::stack<const AstRelation*>& S, std::stack<const AstRelation*>& P, unsigned& numSCCs);

    void printDot(std::ostream& os) const;

    void printJson(std::ostream& os) const;

public:
    static constexpr const char* name = "scc-graph";

    void run(const AstTranslationUnit& translationUnit) override;

    /** Return the number of strongly connected components in the SCC graph */
    size_t size() const {
        return sccToRelation.size();
    }

    /** Get all successor SCCs of a specified scc. */
    const std::set<unsigned>& successorSCCs(const unsigned scc) const {
        return successors.at(scc);
    }

    /** Get all predecessor SCCs of a specified scc. */
    const std::set<unsigned>& predecessorSCCs(const unsigned scc) const {
        return predecessors.at(scc);
    }

    /** Get the relations for a given scc. */
    const std::set<const AstRelation*> relations(const unsigned scc) const {
        return sccToRelation.at(scc);
    }

    /** Get the scc of a given relation. */
    unsigned scc(const AstRelation* relation) const {
        return relationToScc.at(relation);
    }

    /** Get all inbound and all input relations in the given SCC. */
    std::set<const AstRelation*> getIns(const unsigned scc) const {
        auto inbound = getInbound(scc);
        const auto inputs = getInputs(scc);
        inbound.insert(inputs.begin(), inputs.end());
        return inbound;
    }

    /** Get all outbound and all output relations in the given SCC. */
    std::set<const AstRelation*> getOuts(const unsigned scc) const {
        auto outbound = getOutbound(scc);
        const auto outputs = getOutputs(scc);
        outbound.insert(outputs.begin(), outputs.end());
        return outbound;
    }

    /** Get all input relations in the given SCC. */
    std::set<const AstRelation*> getInputs(const unsigned scc) const {
        std::set<const AstRelation*> inputs;
        for (const auto& rel : relations(scc)) {
            if (rel->isInput()) {
                inputs.insert(rel);
            }
        }
        return inputs;
    }

    /** Get all output relations in the given SCC. */
    std::set<const AstRelation*> getOutputs(const unsigned scc) const {
        std::set<const AstRelation*> outputs;
        for (const auto& rel : relations(scc)) {
            if (rel->isOutput()) {
                outputs.insert(rel);
            }
        }
        return outputs;
    }

    /** Get all relations in a predecessor SCC that have a successor relation in the given SCC. */
    std::set<const AstRelation*> getInbound(const unsigned scc) const {
        std::set<const AstRelation*> inbound;
        for (const auto& rel : relations(scc)) {
            for (const auto& pred : precedenceGraph->graph().predecessors(rel)) {
                if ((unsigned)relationToScc.at(pred) != scc) {
                    inbound.insert(pred);
                }
            }
        }
        return inbound;
    }

    /** Get all relations in the given SCC that have a successor relation in another SCC. */
    std::set<const AstRelation*> getOutbound(const unsigned scc) const {
        std::set<const AstRelation*> outbound;
        for (const auto& rel : relations(scc)) {
            for (const auto& succ : precedenceGraph->graph().successors(rel)) {
                if ((unsigned)relationToScc.at(succ) != scc) {
                    outbound.insert(rel);
                    break;
                }
            }
        }
        return outbound;
    }

    /** True if both given relations are in the same SCC, false otherwise. **/
    bool isInSameSCC(const AstRelation* relation1, const AstRelation* relation2) const {
        return relationToScc.at(relation1) == relationToScc.at(relation2);
    }

    bool isRecursive(const unsigned scc) const {
        const std::set<const AstRelation*>& sccRelations = sccToRelation.at(scc);
        if (sccRelations.size() == 1) {
            const AstRelation* singleRelation = *sccRelations.begin();
            if (!precedenceGraph->graph().predecessors(singleRelation).count(singleRelation)) {
                return false;
            }
        }
        return true;
    }

    bool isRecursive(const AstRelation* relation) const {
        return isRecursive(scc(relation));
    }

    /** Output strongly connected component graph in graphviz format */
    void print(std::ostream& os) const override;
};

/**
 * Analysis pass computing a topologically sorted strongly connected component (SCC) graph.
 */
class TopologicallySortedSCCGraph : public AstAnalysis {
private:
    /** The strongly connected component (SCC) graph. */
    SCCGraph* sccGraph;

    /** The final topological ordering of the SCCs. */
    std::vector<unsigned> sccOrder;

    /** Calculate the topological ordering cost of a permutation of as of yet unordered SCCs
    using the ordered SCCs. Returns -1 if the given vector is not a valid topological ordering. */
    unsigned topologicalOrderingCost(const std::vector<unsigned>& permutationOfSCCs) const;

    /** Recursive component for the forwards algorithm computing the topological ordering of the SCCs. */
    void computeTopologicalOrdering(const unsigned scc, std::vector<bool>& visited);

public:
    static constexpr const char* name = "topological-scc-graph";

    void run(const AstTranslationUnit& translationUnit) override;

    const std::vector<unsigned>& order() const {
        return sccOrder;
    }

    /** Output topologically sorted strongly connected component graph in text format */
    void print(std::ostream& os) const override;
};

/**
 * A single step in a relation schedule, consisting of the relations computed in the step
 * and the relations that are no longer required at that step.
 */
class RelationScheduleStep {
private:
    std::set<const AstRelation*> computedRelations;
    std::set<const AstRelation*> expiredRelations;
    const bool isRecursive;

public:
    RelationScheduleStep(std::set<const AstRelation*> computedRelations,
            std::set<const AstRelation*> expiredRelations, const bool isRecursive)
            : computedRelations(computedRelations), expiredRelations(expiredRelations),
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
    friend std::ostream& operator<<(std::ostream& out, const RelationScheduleStep& other) {
        other.print(out);
        return out;
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
    std::vector<RelationScheduleStep> relationSchedule;

    std::vector<std::set<const AstRelation*>> computeRelationExpirySchedule(
            const AstTranslationUnit& translationUnit);

public:
    static constexpr const char* name = "relation-schedule";

    void run(const AstTranslationUnit& translationUnit) override;

    const std::vector<RelationScheduleStep>& schedule() const {
        return relationSchedule;
    }

    /** Dump this relation schedule to standard error. */
    void print(std::ostream& os) const override;
};

}  // end of namespace souffle
