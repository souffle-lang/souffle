/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SCCGraph.cpp
 *
 * Implements method of precedence graph to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#include "ast/analysis/SCCGraph.h"
#include "Global.h"
#include "GraphUtils.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/analysis/AstIOType.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <string>

namespace souffle {

void SCCGraphAnalysis::run(const AstTranslationUnit& translationUnit) {
    precedenceGraph = translationUnit.getAnalysis<PrecedenceGraphAnalysis>();
    ioType = translationUnit.getAnalysis<IOType>();
    sccToRelation.clear();
    relationToScc.clear();
    predecessors.clear();
    successors.clear();

    /* Compute SCC */
    std::vector<AstRelation*> relations = translationUnit.getProgram()->getRelations();
    size_t counter = 0;
    size_t numSCCs = 0;
    std::stack<const AstRelation*> S;
    std::stack<const AstRelation*> P;
    std::map<const AstRelation*, size_t> preOrder;  // Pre-order number of a node (for Gabow's Algo)
    for (const AstRelation* relation : relations) {
        relationToScc[relation] = preOrder[relation] = (size_t)-1;
    }
    for (const AstRelation* relation : relations) {
        if (preOrder[relation] == (size_t)-1) {
            scR(relation, preOrder, counter, S, P, numSCCs);
        }
    }

    /* Build SCC graph */
    successors.resize(numSCCs);
    predecessors.resize(numSCCs);
    for (const AstRelation* u : relations) {
        for (const AstRelation* v : precedenceGraph->graph().predecessors(u)) {
            auto scc_u = relationToScc[u];
            auto scc_v = relationToScc[v];
            assert(scc_u < numSCCs && "Wrong range");
            assert(scc_v < numSCCs && "Wrong range");
            if (scc_u != scc_v) {
                predecessors[scc_u].insert(scc_v);
                successors[scc_v].insert(scc_u);
            }
        }
    }

    /* Store the relations for each SCC */
    sccToRelation.resize(numSCCs);
    for (const AstRelation* relation : relations) {
        sccToRelation[relationToScc[relation]].insert(relation);
    }
}

/* Compute strongly connected components using Gabow's algorithm (cf. Algorithms in
 * Java by Robert Sedgewick / Part 5 / Graph *  algorithms). The algorithm has linear
 * runtime. */
void SCCGraphAnalysis::scR(const AstRelation* w, std::map<const AstRelation*, size_t>& preOrder,
        size_t& counter, std::stack<const AstRelation*>& S, std::stack<const AstRelation*>& P,
        size_t& numSCCs) {
    preOrder[w] = counter++;
    S.push(w);
    P.push(w);
    for (const AstRelation* t : precedenceGraph->graph().predecessors(w)) {
        if (preOrder[t] == (size_t)-1) {
            scR(t, preOrder, counter, S, P, numSCCs);
        } else if (relationToScc[t] == (size_t)-1) {
            while (preOrder[P.top()] > preOrder[t]) {
                P.pop();
            }
        }
    }
    if (P.top() == w) {
        P.pop();
    } else {
        return;
    }

    const AstRelation* v;
    do {
        v = S.top();
        S.pop();
        relationToScc[v] = numSCCs;
    } while (v != w);
    numSCCs++;
}

void SCCGraphAnalysis::print(std::ostream& os) const {
    const std::string& name = Global::config().get("name");
    std::stringstream ss;
    /* Print SCC graph */
    ss << "digraph {" << std::endl;
    /* Print nodes of SCC graph */
    for (size_t scc = 0; scc < getNumberOfSCCs(); scc++) {
        ss << "\t" << name << "_" << scc << "[label = \"";
        ss << join(getInternalRelations(scc), ",\\n",
                [](std::ostream& out, const AstRelation* rel) { out << rel->getQualifiedName(); });
        ss << "\" ];" << std::endl;
    }
    for (size_t scc = 0; scc < getNumberOfSCCs(); scc++) {
        for (auto succ : getSuccessorSCCs(scc)) {
            ss << "\t" << name << "_" << scc << " -> " << name << "_" << succ << ";" << std::endl;
        }
    }
    ss << "}";
    printHTMLGraph(os, ss.str(), getName());
}

}  // end of namespace souffle
