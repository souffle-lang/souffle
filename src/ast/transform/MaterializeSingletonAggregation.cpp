/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MaterializeSingletonAggregation.cpp
 *
 ***********************************************************************/

#include "ast/transform/MaterializeSingletonAggregation.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstAttribute.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

std::string MaterializeSingletonAggregationTransformer::findUniqueVariableName(const AstClause& clause) {
    static int counter = 0;
    std::set<std::string> variableNames;
    visitDepthFirst(clause, [&](const AstVariable& variable) { variableNames.insert(variable.getName()); });
    std::string candidateVariableName = "z";  // completely arbitrary
    while (variableNames.find(candidateVariableName) != variableNames.end()) {
        candidateVariableName = "z" + toString(counter++);
    }
    return candidateVariableName;
}

std::string MaterializeSingletonAggregationTransformer::findUniqueAggregateRelationName(
        const AstProgram& program) {
    static int counter = 0;
    auto candidate = "__agg_rel_" + toString(counter++);
    while (getRelation(program, candidate) != nullptr) {
        candidate = "__agg_rel_" + toString(counter++);
    }
    return candidate;
}

bool MaterializeSingletonAggregationTransformer::transform(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();
    std::set<std::pair<AstAggregator*, AstClause*>> pairs;
    // collect references to clause / aggregate pairs
    visitDepthFirst(program, [&](const AstClause& clause) {
        visitDepthFirst(clause, [&](const AstAggregator& agg) {
            // if the aggregate isn't single valued
            // (ie it relies on a grounding from the outer scope)
            // or it's the only atom in the clause, then there's no point materialising it!
            if (!isSingleValued(agg, clause) || clause.getBodyLiterals().size() == 1) {
                return;
            }
            auto* foundAggregate = const_cast<AstAggregator*>(&agg);
            auto* foundClause = const_cast<AstClause*>(&clause);
            pairs.insert(std::make_pair(foundAggregate, foundClause));
        });
    });
    for (auto pair : pairs) {
        // Clone the aggregate that we're going to be deleting.
        auto aggregate = souffle::clone(pair.first);
        AstClause* clause = pair.second;
        // synthesise an aggregate relation
        // __agg_rel_0()
        auto aggRel = std::make_unique<AstRelation>();
        auto aggHead = std::make_unique<AstAtom>();
        auto aggClause = std::make_unique<AstClause>();

        std::string aggRelName = findUniqueAggregateRelationName(program);
        aggRel->setQualifiedName(aggRelName);
        aggHead->setQualifiedName(aggRelName);

        // create a synthesised variable to replace the aggregate term!
        std::string variableName = findUniqueVariableName(*clause);
        auto variable = std::make_unique<AstVariable>(variableName);

        // __agg_rel_0(z) :- ...
        aggHead->addArgument(souffle::clone(variable));
        aggRel->addAttribute(std::make_unique<AstAttribute>(variableName, "number"));
        aggClause->setHead(souffle::clone(aggHead));

        //    A(x) :- x = sum .., B(x).
        // -> A(x) :- x = z, B(x), __agg_rel_0(z).
        auto equalityLiteral = std::make_unique<AstBinaryConstraint>(
                BinaryConstraintOp::EQ, souffle::clone(variable), souffle::clone(aggregate));
        // __agg_rel_0(z) :- z = sum ...
        aggClause->addToBody(std::move(equalityLiteral));
        program.addRelation(std::move(aggRel));
        program.addClause(std::move(aggClause));

        // the only thing left to do is just replace the aggregate terms in the original
        // clause with the synthesised variable
        struct replaceAggregate : public AstNodeMapper {
            const AstAggregator& aggregate;
            const std::unique_ptr<AstVariable> variable;
            replaceAggregate(const AstAggregator& aggregate, std::unique_ptr<AstVariable> variable)
                    : aggregate(aggregate), variable(std::move(variable)) {}
            std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
                assert(node != nullptr);
                if (auto* current = dynamic_cast<AstAggregator*>(node.get())) {
                    if (*current == aggregate) {
                        auto replacement = souffle::clone(variable);
                        assert(replacement != nullptr);
                        return replacement;
                    }
                }
                node->apply(*this);
                assert(node != nullptr);
                return node;
            }
        };
        replaceAggregate update(*aggregate, std::move(variable));
        clause->apply(update);
        clause->addToBody(std::move(aggHead));
    }
    return pairs.size() > 0;
}

bool MaterializeSingletonAggregationTransformer::isSingleValued(
        const AstAggregator& agg, const AstClause& clause) {
    std::map<std::string, int> occurrences;
    visitDepthFirst(clause, [&](const AstVariable& v) {
        if (occurrences.find(v.getName()) == occurrences.end()) {
            occurrences[v.getName()] = 0;
        }
        occurrences[v.getName()] = occurrences[v.getName()] + 1;
    });
    std::set<std::string> aggVariables;
    visitDepthFirst(agg, [&](const AstVariable& v) {
        aggVariables.insert(v.getName());
        occurrences[v.getName()] = occurrences[v.getName()] - 1;
    });
    for (std::string variableName : aggVariables) {
        if (occurrences[variableName] != 0) {
            return false;
        }
    }
    return true;
}

}  // end of namespace souffle
