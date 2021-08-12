/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SimplifyAggregateTargetExpression.cpp
 *
 ***********************************************************************/

#include "ast/transform/SimplifyAggregateTargetExpression.h"
#include "ast/Argument.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/Aggregate.h"
#include "ast/transform/GroundWitnesses.h"
#include "ast/utility/Visitor.h"

namespace souffle::ast::transform {

Aggregator* SimplifyAggregateTargetExpressionTransformer::simplifyTargetExpression(
        const TranslationUnit& tu, const Clause& clause, Aggregator& aggregator) {
    auto* origTargetExpression = aggregator.getTargetExpression();
    assert(origTargetExpression != nullptr && !isA<Variable>(origTargetExpression) &&
            "aggregator should have complex target expression");

    // Create the new simplified target expression
    auto newTargetExpression = mk<Variable>(analysis::findUniqueVariableName(clause, "x"));

    // Create the new body, with the necessary equality between old and new target expressions
    auto equalityLiteral = mk<BinaryConstraint>(
            BinaryConstraintOp::EQ, clone(newTargetExpression), clone(origTargetExpression));

    VecOwn<Literal> newBody;
    append(newBody, cloneRange(aggregator.getBodyLiterals()));
    newBody.push_back(std::move(equalityLiteral));

    // Variables in the target expression may have been shadowing variables from the outer scope,
    // however, so scoping should be restored if needed. A variable from the TE is shadowing
    // another variable if a variable with the same name appears grounded in the outer scope.

    // e.g. it is possible that this happens:
    // .. :- A(y), x = sum y + z : { B(y, z) }
    // -> :- A(y), x = sum z0: { B(y, z), z0 = y + z }.
    // This is incorrect - the `y` in the target expression should be separated from the `y`
    // grounded in the outer scope.

    // If there are occurrences of the same variable in the outer scope, there are
    // two possible situations:
    // 1) The variable in the outer scope is ungrounded (or occurs in the head)
    //      => This variable is a witness, and should not be renamed, as it is not local
    // 2) The variable in the outer scope is grounded
    //      => This variable is shadowed by a variable in the target expression, should be
    //         renamed

    // Therefore, variables to rename are non-witness outer scope variables
    auto witnesses = analysis::getWitnessVariables(tu, clause, aggregator);
    std::set<std::string> varsOutside = analysis::getVariablesOutsideAggregate(clause, aggregator);

    std::set<std::string> varsGroundedOutside;
    for (auto& varName : varsOutside) {
        if (!contains(witnesses, varName)) {
            varsGroundedOutside.insert(varName);
        }
    }

    // Rename the necessary variables in the new aggregator
    visit(*origTargetExpression, [&](Variable& v) {
        if (contains(varsGroundedOutside, v.getName())) {
            // Rename everywhere in the body to fix scoping
            std::string newVarName = analysis::findUniqueVariableName(clause, v.getName());
            visit(newBody, [&](Variable& literalVar) {
                if (literalVar.getName() == v.getName()) {
                    literalVar.setName(newVarName);
                }
            });
        }
    });

    // Create the new simplified aggregator
    return new Aggregator(aggregator.getBaseOperator(), std::move(newTargetExpression), std::move(newBody));
}

bool SimplifyAggregateTargetExpressionTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    // Helper mapper to replace old aggregators with new versions in a pre-order traversal
    struct replace_aggregators : public NodeMapper {
        const std::map<const Aggregator*, Aggregator*>& oldToNew;

        replace_aggregators(const std::map<const Aggregator*, Aggregator*>& oldToNew) : oldToNew(oldToNew) {}

        std::unique_ptr<Node> operator()(std::unique_ptr<Node> node) const override {
            if (auto* aggregator = as<Aggregator>(node)) {
                if (contains(oldToNew, aggregator)) {
                    return Own<Aggregator>(oldToNew.at(aggregator));
                }
            }
            node->apply(*this);
            return node;
        }
    };

    // Generate the necessary simplified forms for each complex aggregator
    std::map<const Aggregator*, Aggregator*> complexToSimple;
    for (auto* clause : program.getClauses()) {
        visit(*clause, [&](Aggregator& aggregator) {
            const auto* targetExpression = aggregator.getTargetExpression();
            if (targetExpression != nullptr && !isA<Variable>(targetExpression)) {
                complexToSimple.insert(
                        {&aggregator, simplifyTargetExpression(translationUnit, *clause, aggregator)});
            }
        });
    }

    // Replace the old aggregators with the new
    replace_aggregators update(complexToSimple);
    program.apply(update);
    return !complexToSimple.empty();
}

}  // namespace souffle::ast::transform
