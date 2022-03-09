/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MaterializeAggregationQueries.cpp
 *
 ***********************************************************************/

#include "ast/transform/MaterializeAggregationQueries.h"
#include "AggregateOp.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/analysis/Aggregate.h"
#include "ast/analysis/Ground.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapper.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

void MaterializeAggregationQueriesTransformer::instantiateUnnamedVariables(Clause& aggClause) {
    // I should not be fiddling with aggregates that are in the aggregate clause.
    // We can short circuit if we find an aggregate node.
    struct InstantiateUnnamedVariables : public NodeMapper {
        mutable int count = 0;
        Own<Node> operator()(Own<Node> node) const override {
            if (isA<UnnamedVariable>(node)) {
                return mk<Variable>("_" + toString(count++));
            }
            if (isA<Aggregator>(node)) {
                // then DON'T recurse
                return node;
            }
            node->apply(*this);
            return node;
        }
    };

    InstantiateUnnamedVariables update;
    for (const auto& lit : aggClause.getBodyLiterals()) {
        lit->apply(update);
    }
}

std::set<std::string> MaterializeAggregationQueriesTransformer::distinguishHeadArguments(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate) {
    /**
     * The head atom should contain immediate local and injected variables.
     * No witnesses! They have already been transformed away.
     * This means that we exclude any inner aggregate local variables. But
     * we do NOT exclude inner aggregate injected variables!! It's important
     * that the injected variable ends up in this head so that we do not obfuscate
     * the injected variable's relationship to the outer scope.
     * It does not affect the aggregate value because adding an extra column
     * for the injected variable, where that column will only have one value at a time,
     * will essentially replicate the aggregate body relation for as many possible
     * values of the injected variable that there are. The fact that the injected variable
     * will take one value at a time is key.
     **/
    std::set<std::string> headArguments;
    // find local variables of this aggregate and add them
    for (const auto& localVarName : analysis::getLocalVariables(tu, clause, aggregate)) {
        headArguments.insert(localVarName);
    }
    // find local variables of inner aggregate and remove them
    visit(aggregate, [&](const Aggregator& innerAggregate) {
        if (aggregate == innerAggregate) {
            return;
        }
        for (const auto& innerLocalVariableName : analysis::getLocalVariables(tu, clause, innerAggregate)) {
            headArguments.erase(innerLocalVariableName);
        }
    });
    // find injected variables of this aggregate and add them
    for (const auto& injectedVarName : analysis::getInjectedVariables(tu, clause, aggregate)) {
        headArguments.insert(injectedVarName);
    }
    return headArguments;
}

// TODO (Issue 1696): Deal with recursive parameters with an assert statement.
void MaterializeAggregationQueriesTransformer::groundInjectedParameters(
        const TranslationUnit& translationUnit, Clause& aggClause, const Clause& originalClause,
        const Aggregator& aggregate) {
    /**
     *  Mask inner aggregates to make sure we don't consider them grounded and everything.
     **/
    struct NegateAggregateAtoms : public NodeMapper {
        std::unique_ptr<Node> operator()(std::unique_ptr<Node> node) const override {
            if (auto* aggregate = as<Aggregator>(node)) {
                /**
                 * Go through body literals. If the literal is an atom,
                 * then replace the atom with a negated version of the atom, so that
                 * injected parameters that occur in an inner aggregate don't "seem" grounded.
                 **/
                VecOwn<Literal> newBody;
                for (const auto& lit : aggregate->getBodyLiterals()) {
                    if (auto* atom = as<Atom>(lit)) {
                        newBody.push_back(mk<Negation>(clone(atom)));
                    }
                }
                aggregate->setBodyLiterals(std::move(newBody));
            }
            node->apply(*this);
            return node;
        }
    };

    auto aggClauseInnerAggregatesMasked = clone(aggClause);
    aggClauseInnerAggregatesMasked->setHead(mk<Atom>("*"));
    NegateAggregateAtoms update;
    aggClauseInnerAggregatesMasked->apply(update);

    // what is the set of injected variables? Those are the ones we need to ground.
    std::set<std::string> injectedVariables =
            analysis::getInjectedVariables(translationUnit, originalClause, aggregate);

    std::set<std::string> alreadyGrounded;
    for (const auto& argPair : analysis::getGroundedTerms(translationUnit, *aggClauseInnerAggregatesMasked)) {
        const auto* variable = as<ast::Variable>(argPair.first);
        bool variableIsGrounded = argPair.second;
        if (variable == nullptr || variableIsGrounded) {
            continue;
        }
        // If it's not an injected variable, we don't need to ground it
        if (injectedVariables.find(variable->getName()) == injectedVariables.end()) {
            continue;
        }

        std::string ungroundedVariableName = variable->getName();
        if (alreadyGrounded.find(ungroundedVariableName) != alreadyGrounded.end()) {
            // may as well not bother with it because it has already
            // been grounded in a previous iteration
            continue;
        }
        // Try to find any atom in the rule where this ungrounded variable is mentioned
        for (const auto& lit : originalClause.getBodyLiterals()) {
            // -1. This may not be the same literal
            bool originalAggregateFound = false;
            visit(*lit, [&](const Aggregator& a) {
                if (a == aggregate) {
                    originalAggregateFound = true;
                    return;
                }
            });
            if (originalAggregateFound) {
                continue;
            }
            // 0. Variable must not already have been grounded
            if (alreadyGrounded.find(ungroundedVariableName) != alreadyGrounded.end()) {
                continue;
            }
            // 1. Variable must occur in this literal
            bool variableOccursInLit = false;
            visit(*lit, [&](const Variable& var) {
                if (var.getName() == ungroundedVariableName) {
                    variableOccursInLit = true;
                }
            });
            if (!variableOccursInLit) {
                continue;
            }
            // 2. Variable must be grounded by this literal.
            auto singleLiteralClause = mk<Clause>("*");
            singleLiteralClause->addToBody(clone(lit));
            bool variableGroundedByLiteral = false;
            for (const auto& argPair : analysis::getGroundedTerms(translationUnit, *singleLiteralClause)) {
                const auto* var = as<ast::Variable>(argPair.first);
                if (var == nullptr) {
                    continue;
                }
                bool isGrounded = argPair.second;
                if (var->getName() == ungroundedVariableName && isGrounded) {
                    variableGroundedByLiteral = true;
                }
            }
            if (!variableGroundedByLiteral) {
                continue;
            }
            // 3. if it's an atom:
            //  the relation must be of a lower stratum for us to be able to add it. (not implemented)
            //  sanitise the atom by removing any unnecessary arguments that aren't constants
            //  or basically just any other variables
            if (const auto* atom = as<Atom>(lit)) {
                // Right now we only allow things to be grounded by atoms.
                // This is limiting but the case of it being grounded by
                // something else becomes complicated VERY quickly.
                // It may involve pulling in a cascading series of literals like
                // x = y, y = 4. It just seems very painful.
                // remove other unnecessary bloating arguments and replace with an underscore
                VecOwn<Argument> arguments;
                for (auto arg : atom->getArguments()) {
                    if (auto* var = as<ast::Variable>(arg)) {
                        if (var->getName() == ungroundedVariableName) {
                            arguments.emplace_back(clone(arg));
                            continue;
                        }
                    }
                    arguments.emplace_back(new UnnamedVariable());
                }

                auto groundingAtom =
                        mk<Atom>(atom->getQualifiedName(), std::move(arguments), atom->getSrcLoc());
                aggClause.addToBody(clone(groundingAtom));
                alreadyGrounded.insert(ungroundedVariableName);
            }
        }
        assert(alreadyGrounded.find(ungroundedVariableName) != alreadyGrounded.end() &&
                "Error: Unable to ground parameter in materialisation-requiring aggregate body");
        // after this loop, we should have added at least one thing to provide a grounding.
        // If not, we should error out. The program will not be able to run.
        // We have an ungrounded variable that we cannot ground once the aggregate body is
        // outlined.
    }
}

bool MaterializeAggregationQueriesTransformer::materializeAggregationQueries(
        TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();
    /**
     * GENERAL PROCEDURE FOR MATERIALISING THE BODY OF AN AGGREGATE:
     * NB:
     * * Only bodies with more than one atom or an inner aggregate need to be materialised.
     * * Ignore inner aggregates (they will be unwound in subsequent applications of this transformer)
     *
     * * Copy aggregate body literals into a new clause
     * * Pull in grounding atoms
     *
     * * Set up the head: This will include any local and injected variables in the body.
     * * Instantiate unnamed variables in count operation (idk why but it's fine)
     *
     **/
    std::set<const Aggregator*> innerAggregates;
    visitFrontier(program, [&](const Aggregator& agg) {
        visit(agg, [&](const Aggregator& innerAgg) {
            if (agg != innerAgg) {
                innerAggregates.insert(&innerAgg);
            }
        });
        return true;
    });

    for (auto&& cl : program.getClauses()) {
        auto& clause = *cl;
        visit(clause, [&](Aggregator& agg) {
            if (!needsMaterializedRelation(agg)) {
                return;
            }
            // only materialise bottom level aggregates
            if (innerAggregates.find(&agg) != innerAggregates.end()) {
                return;
            }
            // begin materialisation process
            auto aggregateBodyRelationName = analysis::findUniqueRelationName(program, "__agg_subclause");
            // quickly copy in all the literals from the aggregate body
            auto aggClause = mk<Clause>(aggregateBodyRelationName);
            aggClause->setBodyLiterals(clone(agg.getBodyLiterals()));
            if (agg.getBaseOperator() == AggregateOp::COUNT) {
                instantiateUnnamedVariables(*aggClause);
            }
            // pull in any necessary grounding atoms
            groundInjectedParameters(translationUnit, *aggClause, clause, agg);
            // the head must contain all injected/local variables, but not variables
            // local to any inner aggregates. So we'll just take a set minus here.
            // auto aggClauseHead = mk<Atom>(aggregateBodyRelationName);
            auto* aggClauseHead = aggClause->getHead();
            std::set<std::string> headArguments = distinguishHeadArguments(translationUnit, clause, agg);
            // insert the head arguments into the head atom
            for (const auto& variableName : headArguments) {
                aggClauseHead->addArgument(mk<Variable>(variableName));
            }
            // add them to the relation as well (need to do a bit of type analysis to make this work)
            auto aggRel = mk<Relation>(aggregateBodyRelationName);
            std::map<const Argument*, analysis::TypeSet> argTypes =
                    analysis::TypeAnalysis::analyseTypes(translationUnit, *aggClause);

            for (const auto& cur : aggClauseHead->getArguments()) {
                // cur will point us to a particular argument
                // that is found in the aggClause
                auto const curArgType = argTypes[cur];
                assert(!curArgType.empty() && "unexpected empty typeset");
                aggRel->addAttribute(mk<Attribute>(toString(*cur), curArgType.begin()->getName()));
            }

            // Set up the aggregate body atom that will represent the materialised relation we just created
            // and slip in place of the unrestricted literal(s) body.
            // Now it's time to update the aggregate body atom. We can now
            // replace the complex body (with literals) with a body with just the single atom referring
            // to the new relation we just created.
            // all local variables will be replaced by an underscore
            // so we should just quickly fetch the set of local variables for this aggregate.
            auto localVariables = analysis::getLocalVariables(translationUnit, clause, agg);
            if (agg.getTargetExpression() != nullptr) {
                const auto* targetExpressionVariable = as<Variable>(agg.getTargetExpression());
                localVariables.erase(targetExpressionVariable->getName());
            }
            VecOwn<Argument> args;
            for (auto arg : aggClauseHead->getArguments()) {
                if (auto* var = as<ast::Variable>(arg)) {
                    // replace local variable by underscore if local, only injected or
                    // target variables will appear
                    if (localVariables.find(var->getName()) != localVariables.end()) {
                        args.emplace_back(new UnnamedVariable());
                        continue;
                    }
                }
                args.emplace_back(clone(arg));
            }
            auto aggAtom =
                    mk<Atom>(aggClauseHead->getQualifiedName(), std::move(args), aggClauseHead->getSrcLoc());

            VecOwn<Literal> newBody;
            newBody.push_back(std::move(aggAtom));
            agg.setBodyLiterals(std::move(newBody));
            // Now we can just add these new things (relation and its single clause) to the program
            program.addClause(std::move(aggClause));
            program.addRelation(std::move(aggRel));
            changed = true;
        });
    }
    return changed;
}

bool MaterializeAggregationQueriesTransformer::needsMaterializedRelation(const Aggregator& agg) {
    // everything with more than 1 atom  => materialize
    int countAtoms = 0;
    const Atom* atom = nullptr;
    for (const auto& literal : agg.getBodyLiterals()) {
        const Atom* currentAtom = as<Atom>(literal);
        if (currentAtom != nullptr) {
            ++countAtoms;
            atom = currentAtom;
        }
    }

    if (countAtoms > 1) {
        return true;
    }

    bool seenInnerAggregate = false;
    // If we have an aggregate within this aggregate => materialize
    visit(agg, [&](const Aggregator& innerAgg) {
        if (agg != innerAgg) {
            seenInnerAggregate = true;
        }
    });

    if (seenInnerAggregate) {
        return true;
    }

    // If the same variable occurs several times => materialize
    bool duplicates = false;
    std::set<std::string> vars;
    if (atom != nullptr) {
        visit(*atom, [&](const ast::Variable& var) {
            duplicates = duplicates || !vars.insert(var.getName()).second;
        });
    }

    // If there are duplicates a materialization is required
    // for all others the materialization can be skipped
    return duplicates;
}

}  // namespace souffle::ast::transform
