/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SCCGraph.h
 *
 * Defines the class to build the precedence graph,
 * compute strongly connected components of the precedence graph, and
 * build the strongly connected component graph.
 *
 ***********************************************************************/

#pragma once

#include "GraphUtils.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/IOType.h"
#include "ast/analysis/PrecedenceGraph.h"
#include <cstddef>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

namespace souffle::ast {

class TranslationUnit;

namespace analysis {

/**
 * Analysis pass computing the strongly connected component (SCC) graph for the datalog program.
 */
class SCCGraphAnalysis : public Analysis {
public:
    static constexpr const char* name = "scc-graph";

    SCCGraphAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    /** Get the number of SCCs in the graph. */
    std::size_t getNumberOfSCCs() const {
        return sccToRelation.size();
    }

    /** Get the SCC of the given relation. */
    std::size_t getSCC(const Relation* rel) const {
        return relationToScc.at(rel);
    }

    /** Get all successor SCCs of a given SCC. */
    const std::set<std::size_t>& getSuccessorSCCs(const std::size_t scc) const {
        return successors.at(scc);
    }

    /** Get all predecessor SCCs of a given SCC. */
    const std::set<std::size_t>& getPredecessorSCCs(const std::size_t scc) const {
        return predecessors.at(scc);
    }

    /** Get all SCCs containing a successor of a given relation. */
    std::set<std::size_t> getSuccessorSCCs(const Relation* relation) const {
        std::set<std::size_t> successorSccs;
        const auto scc = relationToScc.at(relation);
        for (const auto& successor : precedenceGraph->graph().successors(relation)) {
            const auto successorScc = relationToScc.at(successor);
            if (successorScc != scc) {
                successorSccs.insert(successorScc);
            }
        }
        return successorSccs;
    }

    /** Get all SCCs containing a predecessor of a given relation. */
    std::set<std::size_t> getPredecessorSCCs(const Relation* relation) const {
        std::set<std::size_t> predecessorSccs;
        const auto scc = relationToScc.at(relation);
        for (const auto& predecessor : precedenceGraph->graph().predecessors(relation)) {
            const auto predecessorScc = relationToScc.at(predecessor);
            if (predecessorScc != scc) {
                predecessorSccs.insert(predecessorScc);
            }
        }
        return predecessorSccs;
    }

    /** Get all internal relations of a given SCC. */
    const std::set<const Relation*>& getInternalRelations(const std::size_t scc) const {
        return sccToRelation.at(scc);
    }

    /** Get all external output predecessor relations of a given SCC. */
    std::set<const Relation*> getExternalOutputPredecessorRelations(const std::size_t scc) const {
        std::set<const Relation*> externOutPreds;
        for (const auto& relation : getInternalRelations(scc)) {
            for (const auto& predecessor : precedenceGraph->graph().predecessors(relation)) {
                if (relationToScc.at(predecessor) != scc && ioType->isOutput(predecessor)) {
                    externOutPreds.insert(predecessor);
                }
            }
        }
        return externOutPreds;
    }

    /** Get all external non-output predecessor relations of a given SCC. */
    std::set<const Relation*> getExternalNonOutputPredecessorRelations(const std::size_t scc) const {
        std::set<const Relation*> externNonOutPreds;
        for (const auto& relation : getInternalRelations(scc)) {
            for (const auto& predecessor : precedenceGraph->graph().predecessors(relation)) {
                if (relationToScc.at(predecessor) != scc && !ioType->isOutput(predecessor)) {
                    externNonOutPreds.insert(predecessor);
                }
            }
        }
        return externNonOutPreds;
    }

    /** Get all external predecessor relations of a given SCC. */
    std::set<const Relation*> getExternalPredecessorRelations(const std::size_t scc) const {
        std::set<const Relation*> externPreds;
        for (const auto& relation : getInternalRelations(scc)) {
            for (const auto& predecessor : precedenceGraph->graph().predecessors(relation)) {
                if (relationToScc.at(predecessor) != scc) {
                    externPreds.insert(predecessor);
                }
            }
        }
        return externPreds;
    }

    /** Get all internal output relations of a given SCC. */
    std::set<const Relation*> getInternalOutputRelations(const std::size_t scc) const {
        std::set<const Relation*> internOuts;
        for (const auto& relation : getInternalRelations(scc)) {
            if (ioType->isOutput(relation)) {
                internOuts.insert(relation);
            }
        }
        return internOuts;
    }

    /** Get all internal relations of a given SCC with external successors. */
    std::set<const Relation*> getInternalRelationsWithExternalSuccessors(const std::size_t scc) const {
        std::set<const Relation*> internsWithExternSuccs;
        for (const auto& relation : getInternalRelations(scc)) {
            for (const auto& successor : precedenceGraph->graph().successors(relation)) {
                if (relationToScc.at(successor) != scc) {
                    internsWithExternSuccs.insert(relation);
                    break;
                }
            }
        }
        return internsWithExternSuccs;
    }

    /** Get all internal non-output relations of a given SCC with external successors. */
    std::set<const Relation*> getInternalNonOutputRelationsWithExternalSuccessors(
            const std::size_t scc) const {
        std::set<const Relation*> internNonOutsWithExternSuccs;
        for (const auto& relation : getInternalRelations(scc)) {
            if (!ioType->isOutput(relation)) {
                for (const auto& successor : precedenceGraph->graph().successors(relation)) {
                    if (relationToScc.at(successor) != scc) {
                        internNonOutsWithExternSuccs.insert(relation);
                        break;
                    }
                }
            }
        }
        return internNonOutsWithExternSuccs;
    }

    /** Get all internal input relations of a given SCC. */
    std::set<const Relation*> getInternalInputRelations(const std::size_t scc) const {
        std::set<const Relation*> internIns;
        for (const auto& relation : getInternalRelations(scc)) {
            if (ioType->isInput(relation)) {
                internIns.insert(relation);
            }
        }
        return internIns;
    }

    /** Return if the given SCC is recursive. */
    bool isRecursive(const std::size_t scc) const {
        const std::set<const Relation*>& sccRelations = sccToRelation.at(scc);
        if (sccRelations.size() == 1) {
            const Relation* singleRelation = *sccRelations.begin();
            if (precedenceGraph->graph().predecessors(singleRelation).count(singleRelation) == 0u) {
                return false;
            }
        }
        return true;
    }

    /** Print the SCC graph. */
    void print(std::ostream& os) const override;

private:
    PrecedenceGraphAnalysis* precedenceGraph = nullptr;

    /** Map from node number to SCC number */
    std::map<const Relation*, std::size_t> relationToScc;

    /** Adjacency lists for the SCC graph */
    std::vector<std::set<std::size_t>> successors;

    /** Predecessor set for the SCC graph */
    std::vector<std::set<std::size_t>> predecessors;

    /** Relations contained in a SCC */
    std::vector<std::set<const Relation*>> sccToRelation;

    /** Recursive scR method for computing SCC */
    void scR(const Relation* relation, std::map<const Relation*, std::size_t>& preOrder, std::size_t& counter,
            std::stack<const Relation*>& S, std::stack<const Relation*>& P, std::size_t& numSCCs);

    IOTypeAnalysis* ioType = nullptr;
};

}  // namespace analysis
}  // namespace souffle::ast
