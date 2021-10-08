/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReduceExistentials.cpp
 *
 ***********************************************************************/

#include "ast/transform/ReduceExistentials.h"
#include "GraphUtils.h"
#include "RelationTag.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/ExecutionPlan.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/analysis/IOType.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool ReduceExistentialsTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    // Checks whether an atom is of the form a(_,_,...,_)
    auto isExistentialAtom = [&](const Atom& atom) {
        for (Argument* arg : atom.getArguments()) {
            if (!isA<UnnamedVariable>(arg)) {
                return false;
            }
        }
        return true;
    };

    // Construct a dependency graph G where:
    // - Each relation is a node
    // - An edge (a,b) exists iff a uses b "non-existentially" in one of its *recursive* clauses
    // This way, a relation can be transformed into an existential form
    // if and only if all its predecessors can also be transformed.
    Graph<QualifiedName> relationGraph = Graph<QualifiedName>();

    // Add in the nodes
    for (Relation* relation : program.getRelations()) {
        relationGraph.insert(relation->getQualifiedName());
    }

    // Keep track of all relations that cannot be transformed
    std::set<QualifiedName> minimalIrreducibleRelations;

    auto& ioType = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();

    for (Relation* relation : program.getRelations()) {
        // No I/O relations can be transformed
        if (ioType.isIO(relation)) {
            minimalIrreducibleRelations.insert(relation->getQualifiedName());
        }
        for (Clause* clause : getClauses(program, *relation)) {
            bool recursive = isRecursiveClause(*clause);
            visit(*clause, [&](const Atom& atom) {
                if (atom.getQualifiedName() == clause->getHead()->getQualifiedName()) {
                    return;
                }

                if (!isExistentialAtom(atom)) {
                    if (recursive) {
                        // Clause is recursive, so add an edge to the dependency graph
                        relationGraph.insert(clause->getHead()->getQualifiedName(), atom.getQualifiedName());
                    } else {
                        // Non-existential apperance in a non-recursive clause, so
                        // it's out of the picture
                        minimalIrreducibleRelations.insert(atom.getQualifiedName());
                    }
                }
            });
        }
    }

    // TODO (see issue #564): Don't transform relations appearing in aggregators
    //                        due to aggregator issues with unnamed variables.
    visit(program, [&](const Aggregator& aggr) {
        visit(aggr, [&](const Atom& atom) { minimalIrreducibleRelations.insert(atom.getQualifiedName()); });
    });

    // Run a DFS from each 'bad' source
    // A node is reachable in a DFS from an irreducible node if and only if it is
    // also an irreducible node
    std::set<QualifiedName> irreducibleRelations;
    for (QualifiedName relationName : minimalIrreducibleRelations) {
        relationGraph.visit(
                relationName, [&](const QualifiedName& subRel) { irreducibleRelations.insert(subRel); });
    }

    // All other relations are necessarily existential
    std::set<QualifiedName> existentialRelations;
    for (Relation* relation : program.getRelations()) {
        if (!getClauses(program, *relation).empty() && relation->getArity() != 0 &&
                irreducibleRelations.find(relation->getQualifiedName()) == irreducibleRelations.end()) {
            existentialRelations.insert(relation->getQualifiedName());
        }
    }

    // Reduce the existential relations
    for (QualifiedName relationName : existentialRelations) {
        Relation* originalRelation = getRelation(program, relationName);

        std::stringstream newRelationName;
        newRelationName << "+?exists_" << relationName;

        auto newRelation = mk<Relation>(newRelationName.str(), originalRelation->getSrcLoc());

        // EqRel relations require two arguments, so remove it from the qualifier
        if (newRelation->getRepresentation() == RelationRepresentation::EQREL) {
            newRelation->setRepresentation(RelationRepresentation::DEFAULT);
        }

        // Keep all non-recursive clauses
        for (Clause* clause : getClauses(program, *originalRelation)) {
            if (!isRecursiveClause(*clause)) {
                auto newClause = mk<Clause>(mk<Atom>(newRelationName.str()), clone(clause->getBodyLiterals()),
                        // clone handles nullptr gracefully
                        clone(clause->getExecutionPlan()), clause->getSrcLoc());
                program.addClause(std::move(newClause));
            }
        }

        program.addRelation(std::move(newRelation));
    }

    // Mapper that renames the occurrences of marked relations with their existential
    // counterparts
    struct renameExistentials : public NodeMapper {
        const std::set<QualifiedName>& relations;

        renameExistentials(std::set<QualifiedName>& relations) : relations(relations) {}

        Own<Node> operator()(Own<Node> node) const override {
            if (auto* clause = as<Clause>(node)) {
                if (relations.find(clause->getHead()->getQualifiedName()) != relations.end()) {
                    // Clause is going to be removed, so don't rename it
                    return node;
                }
            } else if (auto* atom = as<Atom>(node)) {
                if (relations.find(atom->getQualifiedName()) != relations.end()) {
                    // Relation is now existential, so rename it
                    std::stringstream newName;
                    newName << "+?exists_" << atom->getQualifiedName();
                    return mk<Atom>(newName.str());
                }
            }
            node->apply(*this);
            return node;
        }
    };

    renameExistentials update(existentialRelations);
    program.apply(update);

    bool changed = !existentialRelations.empty();
    return changed;
}

}  // namespace souffle::ast::transform
