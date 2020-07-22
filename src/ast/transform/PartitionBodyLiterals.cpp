/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PartitionBodyLiteralsTransformer.cpp
 *
 ***********************************************************************/

#include "ast/transform/PartitionBodyLiterals.h"
#include "GraphUtils.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstVisitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

bool PartitionBodyLiteralsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    AstProgram& program = *translationUnit.getProgram();

    /* Process:
     * Go through each clause and construct a variable dependency graph G.
     * The nodes of G are the variables. A path between a and b exists iff
     * a and b appear in a common body literal.
     *
     * Using the graph, we can extract the body literals that are not associated
     * with the arguments in the head atom into new relations. Depending on
     * variable dependencies among these body literals, the literals can
     * be partitioned into multiple separate new propositional clauses.
     *
     * E.g. a(x) <- b(x), c(y), d(y), e(z), f(z). can be transformed into:
     *      - a(x) <- b(x), newrel0(), newrel1().
     *      - newrel0() <- c(y), d(y).
     *      - newrel1() <- e(z), f(z).
     *
     * Note that only one pass through the clauses is needed:
     *  - All arguments in the body literals of the transformed clause cannot be
     *    independent of the head arguments (by construction)
     *  - The new relations holding the disconnected body literals are propositional,
     *    hence having no head arguments, and so the transformation does not apply.
     */

    // Store clauses to add and remove after analysing the program
    std::vector<AstClause*> clausesToAdd;
    std::vector<const AstClause*> clausesToRemove;

    // The transformation is local to each rule, so can visit each independently
    visitDepthFirst(program, [&](const AstClause& clause) {
        // Create the variable dependency graph G
        Graph<std::string> variableGraph = Graph<std::string>();
        std::set<std::string> ruleVariables;

        // Add in the nodes
        // The nodes of G are the variables in the rule
        visitDepthFirst(clause, [&](const AstVariable& var) {
            variableGraph.insert(var.getName());
            ruleVariables.insert(var.getName());
        });

        // Add in the edges
        // Since we are only looking at reachability in the final graph, it is
        // enough to just add in an (undirected) edge from the first variable
        // in the literal to each of the other variables.
        std::vector<AstLiteral*> literalsToConsider = clause.getBodyLiterals();
        literalsToConsider.push_back(clause.getHead());

        for (AstLiteral* clauseLiteral : literalsToConsider) {
            std::set<std::string> literalVariables;

            // Store all variables in the literal
            visitDepthFirst(
                    *clauseLiteral, [&](const AstVariable& var) { literalVariables.insert(var.getName()); });

            // No new edges if only one variable is present
            if (literalVariables.size() > 1) {
                std::string firstVariable = *literalVariables.begin();
                literalVariables.erase(literalVariables.begin());

                // Create the undirected edge
                for (const std::string& var : literalVariables) {
                    variableGraph.insert(firstVariable, var);
                    variableGraph.insert(var, firstVariable);
                }
            }
        }

        // Find the connected components of G
        std::set<std::string> seenNodes;

        // Find the connected component associated with the head
        std::set<std::string> headComponent;
        visitDepthFirst(
                *clause.getHead(), [&](const AstVariable& var) { headComponent.insert(var.getName()); });

        if (!headComponent.empty()) {
            variableGraph.visitDepthFirst(*headComponent.begin(), [&](const std::string& var) {
                headComponent.insert(var);
                seenNodes.insert(var);
            });
        }

        // Compute all other connected components in the graph G
        std::set<std::set<std::string>> connectedComponents;

        for (std::string var : ruleVariables) {
            if (seenNodes.find(var) != seenNodes.end()) {
                // Node has already been added to a connected component
                continue;
            }

            // Construct the connected component
            std::set<std::string> component;
            variableGraph.visitDepthFirst(var, [&](const std::string& child) {
                component.insert(child);
                seenNodes.insert(child);
            });
            connectedComponents.insert(component);
        }

        if (connectedComponents.empty()) {
            // No separate connected components, so no point partitioning
            return;
        }

        // Need to extract some disconnected lits!
        changed = true;
        std::vector<AstAtom*> replacementAtoms;

        // Construct the new rules
        for (const std::set<std::string>& component : connectedComponents) {
            // Come up with a unique new name for the relation
            static int disconnectedCount = 0;
            std::stringstream nextName;
            nextName << "+disconnected" << disconnectedCount;
            AstQualifiedName newRelationName = nextName.str();
            disconnectedCount++;

            // Create the extracted relation and clause for the component
            // newrelX() <- disconnectedLiterals(x).
            auto newRelation = std::make_unique<AstRelation>();
            newRelation->setQualifiedName(newRelationName);
            program.addRelation(std::move(newRelation));

            auto* disconnectedClause = new AstClause();
            disconnectedClause->setSrcLoc(clause.getSrcLoc());
            disconnectedClause->setHead(std::make_unique<AstAtom>(newRelationName));

            // Find the body literals for this connected component
            std::vector<AstLiteral*> associatedLiterals;
            for (AstLiteral* bodyLiteral : clause.getBodyLiterals()) {
                bool associated = false;
                visitDepthFirst(*bodyLiteral, [&](const AstVariable& var) {
                    if (component.find(var.getName()) != component.end()) {
                        associated = true;
                    }
                });
                if (associated) {
                    disconnectedClause->addToBody(souffle::clone(bodyLiteral));
                }
            }

            // Create the atom to replace all these literals
            replacementAtoms.push_back(new AstAtom(newRelationName));

            // Add the clause to the program
            clausesToAdd.push_back(disconnectedClause);
        }

        // Create the replacement clause
        // a(x) <- b(x), c(y), d(z). --> a(x) <- newrel0(), newrel1(), b(x).
        auto* replacementClause = new AstClause();
        replacementClause->setSrcLoc(clause.getSrcLoc());
        replacementClause->setHead(souffle::clone(clause.getHead()));

        // Add the new propositions to the clause first
        for (AstAtom* newAtom : replacementAtoms) {
            replacementClause->addToBody(std::unique_ptr<AstLiteral>(newAtom));
        }

        // Add the remaining body literals to the clause
        for (AstLiteral* bodyLiteral : clause.getBodyLiterals()) {
            bool associated = false;
            bool hasVariables = false;
            visitDepthFirst(*bodyLiteral, [&](const AstVariable& var) {
                hasVariables = true;
                if (headComponent.find(var.getName()) != headComponent.end()) {
                    associated = true;
                }
            });
            if (associated || !hasVariables) {
                replacementClause->addToBody(souffle::clone(bodyLiteral));
            }
        }

        // Replace the old clause with the new one
        clausesToRemove.push_back(&clause);
        clausesToAdd.push_back(replacementClause);
    });

    // Adjust the program
    for (AstClause* newClause : clausesToAdd) {
        program.addClause(std::unique_ptr<AstClause>(newClause));
    }

    for (const AstClause* oldClause : clausesToRemove) {
        program.removeClause(oldClause);
    }

    return changed;
}

}  // end of namespace souffle
