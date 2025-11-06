/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PartitionBodyLiterals.cpp
 *
 ***********************************************************************/

#include "ast/transform/PartitionBodyLiterals.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/utility/Visitor.h"
#include "souffle/datastructure/Graph.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool PartitionBodyLiteralsTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

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
    VecOwn<Clause> clausesToAdd;
    std::vector<const Clause*> clausesToRemove;

    // The transformation is local to each rule, so can visit each independently
    visit(program, [&](const Clause& clause) {
        // Create the variable dependency graph G
        Graph<std::string> variableGraph = Graph<std::string>();
        std::set<std::string> ruleVariables;

        const bool isSubsumption = isA<SubsumptiveClause>(clause);

        // Add in the nodes
        // The nodes of G are the variables in the rule
        visit(clause, [&](const ast::Variable& var) {
            variableGraph.insert(var.getName());
            ruleVariables.insert(var.getName());
        });

        // Add in the edges
        // Since we are only looking at reachability in the final graph, it is
        // enough to just add in an (undirected) edge from the first variable
        // in the literal to each of the other variables.
        std::vector<Literal*> literalsToConsider = clause.getBodyLiterals();
        literalsToConsider.push_back(clause.getHead());

        for (Literal* clauseLiteral : literalsToConsider) {
            std::set<std::string> literalVariables;

            // Store all variables in the literal
            visit(*clauseLiteral, [&](const ast::Variable& var) { literalVariables.insert(var.getName()); });

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
        visit(*clause.getHead(), [&](const ast::Variable& var) { headComponent.insert(var.getName()); });

        if (isSubsumption) {
            // this works because subsumptive clause first two literals are the least and greatest atoms
            // of the original clause's head: `A(least, ... ) <= A(greatest, ...) :- ...`.
            std::set<std::string> leastAtomVariables;
            std::set<std::string> greatestAtomVariables;
            visit(literalsToConsider[0],
                    [&](const ast::Variable& var) { leastAtomVariables.insert(var.getName()); });
            visit(literalsToConsider[1],
                    [&](const ast::Variable& var) { greatestAtomVariables.insert(var.getName()); });
            // Create edge between any couple of variables taken respectively from the least and greatest
            // atoms: `least <-> greatest`, since in the previous step of the algorithm we already linked
            // variables within each atom.
            if (!(leastAtomVariables.empty() || greatestAtomVariables.empty())) {
                const auto vL = *leastAtomVariables.begin();
                const auto vG = *greatestAtomVariables.begin();
                variableGraph.insert(vL, vG);
                variableGraph.insert(vG, vL);
            }
        }

        if (!headComponent.empty()) {
            variableGraph.visit(*headComponent.begin(), [&](const std::string& var) {
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
            variableGraph.visit(var, [&](const std::string& child) {
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
        VecOwn<Literal> replacementAtoms;

        // Construct the new rules
        for (const std::set<std::string>& component : connectedComponents) {
            // Come up with a unique new name for the relation
            static int disconnectedCount = 0;
            std::stringstream nextName;
            nextName << "+disconnected" << disconnectedCount;
            QualifiedName newRelationName = QualifiedName::fromString(nextName.str());
            disconnectedCount++;

            // Create the extracted relation and clause for the component
            // newrelX() <- disconnectedLiterals(x).
            auto newRelation = mk<Relation>(newRelationName);
            program.addRelation(std::move(newRelation));

            auto disconnectedClause = mk<Clause>(newRelationName, clause.getSrcLoc());

            // Find the body literals for this connected component
            std::vector<Literal*> associatedLiterals;
            for (Literal* bodyLiteral : clause.getBodyLiterals()) {
                bool associated = false;
                visit(*bodyLiteral, [&](const ast::Variable& var) {
                    if (component.find(var.getName()) != component.end()) {
                        associated = true;
                    }
                });
                if (associated) {
                    disconnectedClause->addToBody(clone(bodyLiteral));
                }
            }

            // Create the atom to replace all these literals
            replacementAtoms.push_back(mk<Atom>(newRelationName));

            // Add the clause to the program
            clausesToAdd.push_back(std::move(disconnectedClause));
        }

        // Create the replacement clause. The original literals must appear first and in order to
        // satisfy the SubsumptiveClause invariants.
        //
        // `a(x) <- b(x), c(y), d(z). --> a(x) <- b(x), newrel0(), newrel1().`
        Clause* replacementClause;
        if (isSubsumption) {
            replacementClause =
                    new SubsumptiveClause(clone(clause.getHead()), {}, nullptr, clause.getSrcLoc());
        } else {
            replacementClause = new Clause(clone(clause.getHead()), {}, nullptr, clause.getSrcLoc());
        }

        // Add the remaining body literals to the clause
        for (Literal* bodyLiteral : clause.getBodyLiterals()) {
            bool associated = false;
            bool hasVariables = false;
            visit(*bodyLiteral, [&](const ast::Variable& var) {
                hasVariables = true;
                if (headComponent.find(var.getName()) != headComponent.end()) {
                    associated = true;
                }
            });
            if (associated || !hasVariables) {
                replacementClause->addToBody(clone(bodyLiteral));
            }
        }

        replacementClause->addToBody(std::move(replacementAtoms));

        // Replace the old clause with the new one
        clausesToRemove.push_back(&clause);
        clausesToAdd.push_back(std::unique_ptr<Clause>(replacementClause));
    });

    // Adjust the program
    program.addClauses(std::move(clausesToAdd));
    program.removeClauses(clausesToRemove);
    return changed;
}

}  // namespace souffle::ast::transform
