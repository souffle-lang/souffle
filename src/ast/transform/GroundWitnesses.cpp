/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file GroundWitnesses.cpp
 *
 ***********************************************************************/

#include "ast/transform/GroundWitnesses.h"
#include "ast/Aggregator.h"
#include "ast/Clause.h"
#include "ast/analysis/Aggregate.h"
#include "ast/analysis/Ground.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/StringUtil.h"

#include <utility>

namespace souffle::ast::transform {

bool GroundWitnessesTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    struct AggregateWithWitnesses {
        Aggregator* aggregate;
        Clause* originatingClause;
        std::set<std::string> witnesses;

        AggregateWithWitnesses(Aggregator* agg, Clause* clause, std::set<std::string> witnesses)
                : aggregate(agg), originatingClause(clause), witnesses(std::move(witnesses)) {}
    };

    std::vector<AggregateWithWitnesses> aggregatesToFix;

    visit(program, [&](Clause& clause) {
        visit(clause, [&](Aggregator& agg) {
            auto witnessVariables = analysis::getWitnessVariables(translationUnit, clause, agg);
            // remove any witness variables that originate from an inner aggregate
            visit(agg, [&](Aggregator& innerAgg) {
                if (agg == innerAgg) {
                    return;
                }
                auto innerWitnesses = analysis::getWitnessVariables(translationUnit, clause, innerAgg);
                for (const auto& witness : innerWitnesses) {
                    witnessVariables.erase(witness);
                }
            });
            if (witnessVariables.empty()) {
                return;
            }
            AggregateWithWitnesses instance(&agg, &clause, witnessVariables);
            aggregatesToFix.push_back(instance);
        });
    });

    for (struct AggregateWithWitnesses& aggregateToFix : aggregatesToFix) {
        Aggregator* agg = aggregateToFix.aggregate;
        Clause* clause = aggregateToFix.originatingClause;
        std::set<std::string> witnesses = aggregateToFix.witnesses;
        // agg will become invalid when it gets replaced, so we have to be careful
        // not to use it after that point.
        // 1. make a copy of all aggregate body literals, because they will need to
        // be added to the rule body
        VecOwn<Literal> aggregateLiterals;
        for (const auto& literal : agg->getBodyLiterals()) {
            aggregateLiterals.push_back(clone(literal));
        }
        // 1a. TODO: Be sure to rename any INNER witnesses! They have no meaning here and should just be made
        // into (For now I won't allow multi-leveled witnesses) an anonymous variable.

        // 2. Replace witness variables with unique names so they don't clash with the outside
        std::map<std::string, std::string> newWitnessVariableName;
        for (std::string witness : witnesses) {
            newWitnessVariableName[witness] = analysis::findUniqueVariableName(*clause, witness + "_w");
        }
        visit(*agg, [&](Variable& var) {
            if (witnesses.find(var.getName()) != witnesses.end()) {
                // if this variable is a witness, we need to replace it with its new name
                var.setName(newWitnessVariableName[var.getName()]);
            }
        });

        // 3. Replace any instance of the target variable with a clone of the aggregate
        struct TargetVariableReplacer : public NodeMapper {
            Aggregator* aggregate;
            std::string targetVariable;
            TargetVariableReplacer(Aggregator* agg, std::string target)
                    : aggregate(agg), targetVariable(std::move(target)) {}
            std::unique_ptr<Node> operator()(std::unique_ptr<Node> node) const override {
                if (auto* variable = as<Variable>(node)) {
                    if (variable->getName() == targetVariable) {
                        auto replacement = clone(aggregate);
                        return replacement;
                    }
                }
                node->apply(*this);
                return node;
            }
        };
        const auto* targetVariable = as<Variable>(agg->getTargetExpression());
        std::string targetVariableName = targetVariable->getName();
        TargetVariableReplacer replacer(agg, targetVariableName);
        for (std::unique_ptr<Literal>& literal : aggregateLiterals) {
            literal->apply(replacer);
            // 4. Finally add these new grounding literals for the witness
            // to the body of the clause and voila! We've grounded
            // the witness(es)! Yay!
            clause->addToBody(clone(literal));
        }
    }
    return !aggregatesToFix.empty();
}
}  // namespace souffle::ast::transform
