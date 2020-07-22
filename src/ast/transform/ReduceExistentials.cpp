/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReduceExistentialsTransformer.cpp
 *
 ***********************************************************************/

#include "ast/transform/ReduceExistentials.h"
#include "GraphUtils.h"
#include "RelationTag.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/analysis/AstIOTypeAnalysis.h"
#include "souffle/utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

bool ReduceExistentialsTransformer::transform(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();

    // Checks whether an atom is of the form a(_,_,...,_)
    auto isExistentialAtom = [&](const AstAtom& atom) {
        for (AstArgument* arg : atom.getArguments()) {
            if (dynamic_cast<AstUnnamedVariable*>(arg) == nullptr) {
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
    Graph<AstQualifiedName> relationGraph = Graph<AstQualifiedName>();

    // Add in the nodes
    for (AstRelation* relation : program.getRelations()) {
        relationGraph.insert(relation->getQualifiedName());
    }

    // Keep track of all relations that cannot be transformed
    std::set<AstQualifiedName> minimalIrreducibleRelations;

    auto* ioType = translationUnit.getAnalysis<IOType>();

    for (AstRelation* relation : program.getRelations()) {
        // No I/O relations can be transformed
        if (ioType->isIO(relation)) {
            minimalIrreducibleRelations.insert(relation->getQualifiedName());
        }
        for (AstClause* clause : getClauses(program, *relation)) {
            bool recursive = isRecursiveClause(*clause);
            visitDepthFirst(*clause, [&](const AstAtom& atom) {
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
    visitDepthFirst(program, [&](const AstAggregator& aggr) {
        visitDepthFirst(aggr,
                [&](const AstAtom& atom) { minimalIrreducibleRelations.insert(atom.getQualifiedName()); });
    });

    // Run a DFS from each 'bad' source
    // A node is reachable in a DFS from an irreducible node if and only if it is
    // also an irreducible node
    std::set<AstQualifiedName> irreducibleRelations;
    for (AstQualifiedName relationName : minimalIrreducibleRelations) {
        relationGraph.visitDepthFirst(
                relationName, [&](const AstQualifiedName& subRel) { irreducibleRelations.insert(subRel); });
    }

    // All other relations are necessarily existential
    std::set<AstQualifiedName> existentialRelations;
    for (AstRelation* relation : program.getRelations()) {
        if (!getClauses(program, *relation).empty() && relation->getArity() != 0 &&
                irreducibleRelations.find(relation->getQualifiedName()) == irreducibleRelations.end()) {
            existentialRelations.insert(relation->getQualifiedName());
        }
    }

    // Reduce the existential relations
    for (AstQualifiedName relationName : existentialRelations) {
        AstRelation* originalRelation = getRelation(program, relationName);

        std::stringstream newRelationName;
        newRelationName << "+?exists_" << relationName;

        auto newRelation = std::make_unique<AstRelation>();
        newRelation->setQualifiedName(newRelationName.str());
        newRelation->setSrcLoc(originalRelation->getSrcLoc());

        // EqRel relations require two arguments, so remove it from the qualifier
        if (newRelation->getRepresentation() == RelationRepresentation::EQREL) {
            newRelation->setRepresentation(RelationRepresentation::DEFAULT);
        }

        // Keep all non-recursive clauses
        for (AstClause* clause : getClauses(program, *originalRelation)) {
            if (!isRecursiveClause(*clause)) {
                auto newClause = std::make_unique<AstClause>();

                newClause->setSrcLoc(clause->getSrcLoc());
                if (const AstExecutionPlan* plan = clause->getExecutionPlan()) {
                    newClause->setExecutionPlan(souffle::clone(plan));
                }
                newClause->setHead(std::make_unique<AstAtom>(newRelationName.str()));
                for (AstLiteral* lit : clause->getBodyLiterals()) {
                    newClause->addToBody(souffle::clone(lit));
                }

                program.addClause(std::move(newClause));
            }
        }

        program.addRelation(std::move(newRelation));
    }

    // Mapper that renames the occurrences of marked relations with their existential
    // counterparts
    struct renameExistentials : public AstNodeMapper {
        const std::set<AstQualifiedName>& relations;

        renameExistentials(std::set<AstQualifiedName>& relations) : relations(relations) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* clause = dynamic_cast<AstClause*>(node.get())) {
                if (relations.find(clause->getHead()->getQualifiedName()) != relations.end()) {
                    // Clause is going to be removed, so don't rename it
                    return node;
                }
            } else if (auto* atom = dynamic_cast<AstAtom*>(node.get())) {
                if (relations.find(atom->getQualifiedName()) != relations.end()) {
                    // Relation is now existential, so rename it
                    std::stringstream newName;
                    newName << "+?exists_" << atom->getQualifiedName();
                    return std::make_unique<AstAtom>(newName.str());
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

}  // end of namespace souffle
