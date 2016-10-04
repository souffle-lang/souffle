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
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "PrecedenceGraph.h"

#include <set>
#include <algorithm>

#include "AstRelation.h"
#include "AstClause.h"
#include "AstVisitor.h"
#include "AstUtils.h"

namespace souffle {

void PrecedenceGraph::run(const AstTranslationUnit& translationUnit) {
    /* Get relations */
    std::vector<AstRelation *> relations = translationUnit.getProgram()->getRelations();

    for (AstRelation* r : relations) {
        precedenceGraph.addNode(r);
        for (size_t i = 0; i < r->clauseSize(); i++) {
            AstClause *c = r->getClause(i);
            const std::set<const AstRelation *> &dependencies = getBodyRelations(c, translationUnit.getProgram());
            for(std::set<const AstRelation *>::const_iterator irs = dependencies.begin(); irs != dependencies.end(); ++irs) {
                const AstRelation *source = (*irs);
                precedenceGraph.addEdge(r, source);
            }
        }
    }
}

void PrecedenceGraph::outputPrecedenceGraph(std::ostream& os) {
    /* Print dependency graph */
    os << "digraph \"dependence-graph\" {\n";
    /* Print node of dependence graph */
    unsigned int u = 0;
    for (const AstRelation *rel : precedenceGraph.getNodes()) {
        os << "\t\"" << rel->getName() << "\" [label = \"" << rel->getName() << "\"];\n";
        u++;
    }

    for (const AstRelation *rel : precedenceGraph.getNodes()) {
        for(const AstRelation *adjRel : precedenceGraph.getEdges(rel)) {
            os << "\t\"" << adjRel->getName() << "\" -> \"" << rel->getName() << "\";\n";
        }
    }

    os << "}\n";
}

void RedundantRelations::run(const AstTranslationUnit& translationUnit) {
    precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();

    std::set<const AstRelation *> work;
    std::set<const AstRelation *> notRedundant;

    const std::vector<AstRelation *> &relations = translationUnit.getProgram()->getRelations();

    /* Add all output relations to the work set */
    for (const AstRelation* r : relations) {
        if(r->isComputed()) {
            work.insert(r);
        }
    }

    /* Find all relations which are not redundant for the computations of the
       output relations. */
    while(!work.empty()){
        /* Chose one element in the work set and add it to notRedundant */
        const AstRelation *u = *(work.begin());
        work.erase(work.begin());
        notRedundant.insert(u);

        /* Find all predecessors of u and add them to the worklist
            if they are not in the set notRedundant */
        for (const AstRelation *predecessor : precedenceGraph->getPredecessors(u)) {

            if (!notRedundant.count(predecessor)) {
                work.insert(predecessor);
            }
        }

    }

    /* All remaining relations are redundant. */
    redundantRelations.clear();
    for (const AstRelation *r : relations) {
        if (!notRedundant.count(r)) {
            redundantRelations.insert(r);
        }
    }
}

void RecursiveClauses::run(const AstTranslationUnit& translationUnit) {
    visitDepthFirst(*translationUnit.getProgram(), [&](const AstClause& clause) {
        if (computeIsRecursive(clause, translationUnit)) {
            recursiveClauses.insert(&clause);
        }
    });
}

bool RecursiveClauses::computeIsRecursive(const AstClause& clause, const AstTranslationUnit& translationUnit) const {
    const AstProgram &program = *translationUnit.getProgram();

    // we want to reach the atom of the head through the body
    const AstRelation* trg = getHeadRelation(&clause, &program);

    std::set<const AstRelation*> reached;
    std::vector<const AstRelation*> worklist;

    // set up start list
    for(const AstAtom* cur : clause.getAtoms()) {
        auto rel = program.getRelation(cur->getName());
        if (rel == trg) return true;
        worklist.push_back(rel);
    }

    // process remaining elements
    while(!worklist.empty()) {
        // get next to process
        const AstRelation* cur = worklist.back();
        worklist.pop_back();

        // skip null pointers (errors in the input code)
        if (!cur) continue;

        // check whether this one has been checked before
        if (!reached.insert(cur).second) continue;

        // check all atoms in the relations
        for(const AstClause* cl : cur->getClauses()) {
            for(const AstAtom* at : cl->getAtoms()) {
                auto rel = program.getRelation(at->getName());
                if (rel == trg) return true;
                worklist.push_back(rel);
            }
        }
    }

    // no cycles found
    return false;

}

void SCCGraph::run(const AstTranslationUnit& translationUnit) {
    precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();
    SCC.clear();
    nodeToSCC.clear();
    predSCC.clear();
    succSCC.clear();

    /* Compute SCC */
    std::vector<AstRelation *> relations = translationUnit.getProgram()->getRelations();
    unsigned int counter = 0;
    int numSCCs = 0;
    std::stack<const AstRelation *> S, P;
    std::map<const AstRelation *, int> preOrder; // Pre-order number of a node (for Gabow's Algo)
    for (const AstRelation *relation : relations) {
        nodeToSCC[relation] = preOrder[relation] = -1;
    }
    for (const AstRelation *relation : relations) {
        if (preOrder[relation]  == -1) {
            scR(relation, preOrder, counter, S, P, numSCCs);
        }
    }

    /* Build SCC graph */
    succSCC.resize(numSCCs);
    predSCC.resize(numSCCs);
    for (const AstRelation *u : relations) {
        for (const AstRelation *v : precedenceGraph->getPredecessors(u)) {
            int scc_u = nodeToSCC[u];
            int scc_v = nodeToSCC[v];
            ASSERT(scc_u >= 0 && scc_u < numSCCs && "Wrong range");
            ASSERT(scc_v >= 0 && scc_v < numSCCs && "Wrong range");
            if (scc_u != scc_v) {
                predSCC[scc_u].insert(scc_v);
                succSCC[scc_v].insert(scc_u);
            }
        }
    }

    /* Store the relations for each SCC */
    SCC.resize(numSCCs);
    for (const AstRelation *relation : relations) {
        SCC[nodeToSCC[relation]].insert(relation);
    }

}

/* Compute strongly connected components using Gabow's algorithm (cf. Algorithms in
 * Java by Robert Sedgewick / Part 5 / Graph *  algorithms). The algorithm has linear
 * runtime. */
void SCCGraph::scR(const AstRelation *w, std::map<const AstRelation *, int> &preOrder, unsigned int &counter,
        std::stack<const AstRelation *> &S, std::stack<const AstRelation *> &P, int &numSCCs) {

    preOrder[w] = counter++;
    S.push(w);
    P.push(w);
    for(const AstRelation *t : precedenceGraph->getPredecessors(w)) {
        if (preOrder[t] == -1) {
            scR(t, preOrder, counter, S, P, numSCCs);
        } else if (nodeToSCC[t] == -1) {
            while (preOrder[P.top()] > preOrder[t]) {
                P.pop();
            }
        }
    }
    if (P.top() == w) {
        P.pop();
    } else
        return;

    const AstRelation *v;
    do {
        v = S.top();
        S.pop();
        nodeToSCC[v] = numSCCs;
    } while(v != w);
    numSCCs++;
}

void SCCGraph::outputSCCGraph(std::ostream& os) {
    /* Print SCC graph */
    os << "digraph \"scc-graph\" {\n";
    /* Print nodes of SCC graph */
    int numSCCs = getNumSCCs();
    for(int scc = 0; scc < numSCCs; scc++) {
        os << "\t snode" << scc << "[label = \"";
        os << join(getRelationsForSCC(scc), ",\\n", [](std::ostream& out, const AstRelation* rel) {
            out << rel->getName();
        });
        os << "\",color=black];\n";
    }

    /* Print edges of SCC graph */
    for(int scc = 0; scc < numSCCs; scc++) {
        for (int successor : getSuccessorSCCs(scc)) {
            os << "\tsnode" << scc << " -> snode" << successor << ";\n";
        }
    }
    os << "}\n";
}

/* Compute the topsort of the SCC graph using a reverse DFS and markers */
void TopologicallySortedSCCGraph::reverseDFS(int sv, std::vector<enum Colour>& sccMarkers) {
    if (sccMarkers[sv] == GRAY) {
        assert("SCC graph is not a DAG");
    } else if (sccMarkers[sv] == WHITE) {
        sccMarkers[sv] = GRAY;
        for (int scc : sccGraph->getPredecessorSCCs(sv)) {
            reverseDFS(scc, sccMarkers);
        }
        sccMarkers[sv] = BLACK;
        orderedSCCs.push_back(sv);
    }
}

const int TopologicallySortedSCCGraph::topologicalOrderingCost(const std::vector<int>& permutationOfSCCs) const {
    // create variables to hold the cost of the current SCC and the permutation as a whole
    int costOfSCC = 0;
    int costOfPermutation = -1;
    // obtain an iterator to the end of the already ordered partition of sccs
    auto it_k = permutationOfSCCs.begin() + orderedSCCs.size();
    // for each of the scc's in the ordering, resetting the cost of the scc to zero on each loop
    for (auto it_i = permutationOfSCCs.begin(); it_i != permutationOfSCCs.end(); ++it_i, costOfSCC = 0) {
        // if the index of the current scc is after the end of the ordered partition
        if (it_i >= it_k)
            // check that the index of all predecessor sccs of are before the index of the current scc
            for (int scc : sccGraph->getPredecessorSCCs(*it_i))
                if (std::find(permutationOfSCCs.begin(), it_i, scc) == it_i)
                    // if not, the sort is not a valid topological sort
                    return -1;
        // otherwise, calculate the cost of the current scc
        // as the number of sccs with an index before the current scc
        for (auto it_j = permutationOfSCCs.begin(); it_j != it_i; ++it_j)
            // having some successor scc with an index after the current scc
            for (int scc : sccGraph->getSuccessorSCCs(*it_j))
                if (std::find(permutationOfSCCs.begin(), it_i, scc) == it_i)
                      costOfSCC++;
        // and if this cost is greater than the maximum recorded cost for the whole permutation so far,
        // set the cost of the permutation to it
        if (costOfSCC > costOfPermutation)
            costOfPermutation = costOfSCC;
    }
    return costOfPermutation;
}

void TopologicallySortedSCCGraph::bestCostTopologicalOrdering(std::deque<int>& lookaheadSCCs) const {
    // exit early if there is only one scc in the lookahead
    if (lookaheadSCCs.size() == 1)
        return;
    // otherwise, create a vector to hold a permutation of the lookahead sccs and to store the best
    // cost permutation so far
    int bestCostOfPermutation = -1;
    std::vector<int> permutationOfSCCs = orderedSCCs;
    std::deque<int> bestPermutationOfSCCs;
    // sort the lookahead scc's
    std::sort(lookaheadSCCs.begin(), lookaheadSCCs.end());
    // then iterate through all permutations of them
    do {
        // erase the previous permutation of lookahead scc's from the current permutation, if it exists
        if (permutationOfSCCs.size() > orderedSCCs.size())
            permutationOfSCCs.erase(permutationOfSCCs.begin() + orderedSCCs.size(), permutationOfSCCs.end());
        // then add the next permutation of lookahead scc's to the current permutation of sccs
        permutationOfSCCs.insert(permutationOfSCCs.end(), lookaheadSCCs.begin(), lookaheadSCCs.end());
        // obtain the cost of the ordering of the current permutation of sccs
        int costOfPermutation = topologicalOrderingCost(permutationOfSCCs);
        // if the ordering is not topologically valid, do nothing
        if (costOfPermutation == -1)
            continue;
        // otherwise, if this cost is better than the best cost so far
        if (costOfPermutation < bestCostOfPermutation || bestCostOfPermutation == -1) {
            // record it as the best cost and this ordering as the best ordering
            bestCostOfPermutation = costOfPermutation;
            bestPermutationOfSCCs = lookaheadSCCs;
        }
    } while (std::next_permutation(lookaheadSCCs.begin(), lookaheadSCCs.end()));
    // finally, set the lookahead scc's to the best cost ordering
    lookaheadSCCs = bestPermutationOfSCCs;
}

void TopologicallySortedSCCGraph::khansAlgorithm(std::deque<int>& lookaheadSCCs, std::vector<enum Colour>& sccMarkers) {
    // establish lists for the current and next round of sccs
    std::deque<int> currentRoundOfSCCs = lookaheadSCCs;
    std::deque<int> nextRoundOfSCCs;
    // while more sccs remain for the current round
    while (!currentRoundOfSCCs.empty()) {
        // clear the list of lookahead scc's
        lookaheadSCCs.clear();
        // for LOOKAHEAD number of levels
        for (unsigned int i = 0; i < LOOKAHEAD; ++i) {
            // clear the list of scc's for the next round
            nextRoundOfSCCs.clear();
            // while scc's remain for the current round
            while (!currentRoundOfSCCs.empty()) {
                // get the last added scc of the current round
                int scc_i = currentRoundOfSCCs.back();
                currentRoundOfSCCs.pop_back();
                // give it a permanent mark
                sccMarkers[scc_i] = BLACK;
                // add it to the list of lookahead scc's
                lookaheadSCCs.push_back(scc_i);
                // and for each successor of that scc
                for (int scc_j : sccGraph->getSuccessorSCCs(scc_i)) {
                    // check it has not yet been visited
                    if (sccMarkers[scc_j] != WHITE)
                        continue;
                    // check that it has no predecessors which have not been visited
                    bool hasUnvisitedPredecessor = false;
                    for (int scc_k : sccGraph->getPredecessorSCCs(scc_j)) {
                        if (sccMarkers[scc_k] != BLACK) {
                            hasUnvisitedPredecessor = true;
                            break;
                        }
                    }
                    if (hasUnvisitedPredecessor) continue;
                    // if it passes, give it a temporary mark
                    sccMarkers[scc_j] = GRAY;
                    // and add it to the list of scc's for the next round
                    nextRoundOfSCCs.push_back(scc_j);
                }
            }
            // set the sccs for the current round to the sccs for the next round
            currentRoundOfSCCs = nextRoundOfSCCs;
        }
        // compute the best cost topological ordering over the set of lookahead sccs
        bestCostTopologicalOrdering(lookaheadSCCs);
        // and append it to the final list of ordered sccs
        orderedSCCs.insert(orderedSCCs.end(), lookaheadSCCs.begin(), lookaheadSCCs.end());
    }
    lookaheadSCCs.clear();
}

void TopologicallySortedSCCGraph::runReverseDFS(std::vector<enum Colour>& sccMarkers) {
    // run reverse DFS for each node in the scc graph
    for (int su = 0; su < sccGraph->getNumSCCs(); ++su) {
        reverseDFS(su, sccMarkers);
    }
}

void TopologicallySortedSCCGraph::runKhansAlgorithm(std::vector<enum Colour>& sccMarkers) {
    std::deque<int> lookaheadSCCs;
    // for each of the sccs in the graph
    for (int scc = 0; scc < sccGraph->getNumSCCs(); ++scc) {
        // if that scc has no predecessors
        if (sccGraph->getPredecessorSCCs(scc).empty()) {
            // and if the scc has no successors either
            if (sccGraph->getSuccessorSCCs(scc).empty()) {
                sccMarkers[scc] = BLACK;
                // put it in the ordering
                orderedSCCs.push_back(scc);
            // otherwise, if the scc only has no predecessors
            } else {
                // run khan's algorithm using the current scc as a start node
                sccMarkers[scc] = GRAY;
                lookaheadSCCs.push_back(scc);
                khansAlgorithm(lookaheadSCCs, sccMarkers);
            }
        }
    }
    // finally, check that all nodes have been visited
    if (std::find(sccMarkers.begin(), sccMarkers.end(), WHITE) != sccMarkers.end()) {
        assert("SCC graph is not a DAG");
    }
}

void TopologicallySortedSCCGraph::run(const AstTranslationUnit& translationUnit) {
    sccGraph = translationUnit.getAnalysis<SCCGraph>();

    /* Compute topological sort for the SCC graph */
    orderedSCCs.clear();
    std::vector<enum Colour> sccMarkers; // Markers of a SCC for topsort
    sccMarkers.resize(sccGraph->getNumSCCs(), WHITE);

    // runReverseDFS(sccMarkers); // Topsort using reverse DFS algorithm
    runKhansAlgorithm(sccMarkers); // Topsort using Khan's algorithm
}

void TopologicallySortedSCCGraph::outputTopologicallySortedSCCGraph(std::ostream& os) {
    int numSCCs = orderedSCCs.size();
    for (int i = 0; i < numSCCs; i++) {
        os << "[";
        os << join(sccGraph->getRelationsForSCC(orderedSCCs[i]), ", ", [](std::ostream& out, const AstRelation* rel) {
            out << rel->getName();
        });
        os << "]\n";
    }
}

void RelationSchedule::run(const AstTranslationUnit& translationUnit) {
    topsortSCCGraph = translationUnit.getAnalysis<TopologicallySortedSCCGraph>();
    precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();

    int numSCCs = topsortSCCGraph->getSCCGraph()->getNumSCCs();
    std::vector<std::set<const AstRelation *>> relationExpirySchedule = computeRelationExpirySchedule(translationUnit);

    schedule.clear();
    for (int i = 0; i < numSCCs; i++) {
        int scc = topsortSCCGraph->getSCCOrder()[i];
        const std::set<const AstRelation *> computedRelations = topsortSCCGraph->getSCCGraph()->getRelationsForSCC(scc);
        schedule.emplace_back(computedRelations, relationExpirySchedule[i], topsortSCCGraph->getSCCGraph()->isRecursive(scc));
    }
}

std::vector<std::set<const AstRelation *>> RelationSchedule::computeRelationExpirySchedule(const AstTranslationUnit &translationUnit) {
    std::vector<std::set<const AstRelation *>> relationExpirySchedule;
    /* Compute for each step in the reverse topological order
       of evaluating the SCC the set of alive relations. */

    int numSCCs = topsortSCCGraph->getSCCOrder().size();

    /* Alive set for each step */
    std::vector< std::set<const AstRelation *> > alive(numSCCs);
    /* Resize expired relations sets */
    relationExpirySchedule.resize(numSCCs);

    /* Mark the output relations as alive in the first step */
    for (const AstRelation *relation : translationUnit.getProgram()->getRelations()) {
        if (relation->isComputed()) {
            alive[0].insert(relation);
        }
    }

    /* Compute all alive relations by iterating over all steps in reverse order
       determine the dependencies */
    for(int orderedSCC = 1; orderedSCC < numSCCs; orderedSCC++) {
        /* Add alive set of previous step */
        alive[orderedSCC].insert(alive[orderedSCC-1].begin(), alive[orderedSCC-1].end());

        /* Add predecessors of relations computed in this step */
        int scc = topsortSCCGraph->getSCCOrder()[numSCCs - orderedSCC];
        for (const AstRelation *r : topsortSCCGraph->getSCCGraph()->getRelationsForSCC(scc)) {
            for (const AstRelation *predecessor : precedenceGraph->getPredecessors(r)) {
                alive[orderedSCC].insert(predecessor);
            }
        }

        /* Compute expired relations in reverse topological order using the set difference of the alive sets
           between steps. */
        std::set_difference(alive[orderedSCC].begin(), alive[orderedSCC].end(),
                alive[orderedSCC-1].begin(), alive[orderedSCC-1].end(),
                std::inserter(relationExpirySchedule[numSCCs-orderedSCC], relationExpirySchedule[numSCCs-orderedSCC].end()));
    }

    return relationExpirySchedule;
}

} // end of namespace souffle

