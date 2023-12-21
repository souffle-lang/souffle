/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Ground.cpp
 *
 * Implements Aggregate Analysis methods to determine scope of variables
 *
 ***********************************************************************/

#include "ast/analysis/Aggregate.h"
#include "ast/Aggregator.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/Ground.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/StringUtil.h"

#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

/**
 *  Computes the set of local variables
 *  in an aggregate expression.
 *  This is just the set of all variables occurring in the aggregate
 *  MINUS the injected variables
 *  MINUS the witness variables. :)
 *
 **/
std::set<std::string> getLocalVariables(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate) {
    std::set<std::string> allVariablesInAggregate;
    visit(aggregate, [&](const Variable& v) { allVariablesInAggregate.insert(v.getName()); });
    std::set<std::string> injectedVariables = getInjectedVariables(tu, clause, aggregate);
    std::set<std::string> witnessVariables = getWitnessVariables(tu, clause, aggregate);
    std::set<std::string> localVariables;
    for (const std::string& name : allVariablesInAggregate) {
        if (injectedVariables.find(name) == injectedVariables.end() &&
                witnessVariables.find(name) == witnessVariables.end()) {
            localVariables.insert(name);
        }
    }
    return localVariables;
}
/**
 * Computes the set of witness variables that are used in the aggregate
 * A variable is a witness if it occurs in the aggregate body (but not in an inner aggregate)
 * and also occurs ungrounded in the outer scope.
 **/
std::set<std::string> getWitnessVariables(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate) {
    // 1. Create an aggregatorless clause so that we can analyse the groundings
    // in the rule scope. This is one where we replace every aggregator with
    // a variable, and then provide a grounding for that variable.
    struct M : public NodeMapper {
        // Variables introduced to replace aggregators
        mutable std::set<std::string> aggregatorVariables;

        const std::set<std::string>& getAggregatorVariables() {
            return aggregatorVariables;
        }

        std::unique_ptr<Node> operator()(std::unique_ptr<Node> node) const override {
            static int numReplaced = 0;
            if (isA<Aggregator>(node)) {
                // Replace the aggregator with a variable
                std::stringstream newVariableName;
                newVariableName << "+aggr_var_" << numReplaced++;

                // Keep track of which variables are bound to aggregators
                aggregatorVariables.insert(newVariableName.str());

                return mk<Variable>(newVariableName.str());
            }
            node->apply(*this);
            return node;
        }
    };

    auto aggregatorlessClause = mk<Clause>(QualifiedName::fromString("*"));
    aggregatorlessClause->setBodyLiterals(clone(clause.getBodyLiterals()));

    auto negatedHead = mk<Negation>(clone(clause.getHead()));
    aggregatorlessClause->addToBody(std::move(negatedHead));

    // Replace all aggregates with variables
    M update;
    aggregatorlessClause->apply(update);
    auto groundingAtom = mk<Atom>(QualifiedName::fromString("+grounding_atom"));
    for (const std::string& variableName : update.getAggregatorVariables()) {
        groundingAtom->addArgument(mk<Variable>(variableName));
    }
    aggregatorlessClause->addToBody(std::move(groundingAtom));
    // 2. Create an aggregate clause so that we can check
    // that it IS this aggregate giving a grounding to the candidate variable.
    auto aggregateSubclause = mk<Clause>(QualifiedName::fromString("*"));
    aggregateSubclause->setBodyLiterals(clone(aggregate.getBodyLiterals()));

    std::set<std::string> witnessVariables;
    auto isGroundedInAggregateSubclause = analysis::getGroundedTerms(tu, *aggregateSubclause);
    // 3. Calculate all the witness variables
    // A witness will occur ungrounded in the aggregatorlessClause
    for (const auto& argPair : analysis::getGroundedTerms(tu, *aggregatorlessClause)) {
        if (const auto* variable = as<Variable>(argPair.first)) {
            bool variableIsGrounded = argPair.second;
            if (!variableIsGrounded) {
                // then we expect it to be grounded in the aggregate subclause
                // if it's a witness!!
                for (const auto& aggArgPair : isGroundedInAggregateSubclause) {
                    if (const auto* var = as<Variable>(aggArgPair.first)) {
                        bool aggVariableIsGrounded = aggArgPair.second;
                        if (var->getName() == variable->getName() && aggVariableIsGrounded) {
                            witnessVariables.insert(variable->getName());
                        }
                    }
                }
            }
        }
    }
    // 4. A witness variable may actually "originate" from an outer scope and
    // just have been injected into this inner aggregate. Just check the set of injected variables
    // and quickly minus them out.
    std::set<std::string> injectedVariables = analysis::getInjectedVariables(tu, clause, aggregate);
    for (const std::string& injected : injectedVariables) {
        witnessVariables.erase(injected);
    }

    return witnessVariables;
}
/**
 *  Computes the set of variables occurring outside the aggregate
 **/
std::set<std::string> getVariablesOutsideAggregate(const Clause& clause, const Aggregator& aggregate) {
    std::map<std::string, int> variableOccurrences;
    visit(clause, [&](const Variable& var) { variableOccurrences[var.getName()]++; });
    visit(aggregate, [&](const Variable& var) { variableOccurrences[var.getName()]--; });
    std::set<std::string> variablesOutsideAggregate;
    for (auto const& pair : variableOccurrences) {
        std::string v = pair.first;
        int numOccurrences = pair.second;
        if (numOccurrences > 0) {
            variablesOutsideAggregate.insert(v);
        }
    }
    return variablesOutsideAggregate;
}

std::string findUniqueVariableName(const Clause& clause, const std::string& base) {
    std::set<std::string> variablesInClause;
    visit(clause, [&](const Variable& v) { variablesInClause.insert(v.getName()); });
    int varNum = 0;
    std::string candidate = base;
    while (variablesInClause.find(candidate) != variablesInClause.end()) {
        candidate = base + toString(varNum++);
    }
    return candidate;
}

std::string findUniqueRelationName(const Program& program, const std::string& base) {
    int counter = 0;
    auto candidate = base;
    while (program.getRelation(QualifiedName::fromString(candidate)) != nullptr) {
        candidate = base + toString(counter++);
    }
    return candidate;
}

/**
 *  Given an aggregate and a clause, we find all the variables that have been
 *  injected into the aggregate.
 *  This means that the variable occurs grounded in an outer scope.
 *  BUT does not occur in the target expression.
 **/
std::set<std::string> getInjectedVariables(
        const TranslationUnit& tu, const Clause& clause, const Aggregator& aggregate) {
    /**
     *  Imagine we have this:
     *
     *   A(letter, x) :-
     *       a(letter, _),
     *       x = min y : { a(l, y),
     *       y < max y : { a(letter, y) } },
     *       z = max y : {B(y) }.
     *
     *   We need to figure out the base literal in which this aggregate occurs,
     *   so that we don't delete them
     *   Other aggregates occurring in the rule are irrelevant, so we'll replace
     *   the aggregate with a dummy aggregate variable. Then after that,
     *
     *   2. A(letter, x) :-
     *       a(letter, _),
     *       x = min y : { a(l, y),
     *       y < max y : { a(letter, y) } },
     *       z = aggr_var_0.
     *
     *   We should also ground all the aggr_vars so that this grounding
     *   transfers to whatever variable they might be assigned to.
     *
     *   A(letter, x) :-
     *       a(letter, _),
     *       x = min y : { a(l, y),
     *       y < max y : { a(letter, y) } },
     *       z = aggr_var_0,
     *       grounding_atom(aggr_var_0).
     *
     *   Remember to negate the head atom and add it to the body. The clause we deal with will have a
     *   dummy head (*())
     *
     *    *() :- !A(letter, x), a(letter, _), x = min y : { a(l, y), y < max y : { a(letter, y) } },
     *        z = aggr_var_0, grounding_atom(aggr_var_0).
     *
     *   Replace the original aggregate with the same shit because we can't be counting local variables n shit
     *
     *    *() :- !A(letter, x), a(letter, _), x = min y : { a(l, y), y < aggr_var_1 } }, z = aggr_var_0,
     *        grounding_atom(aggr_var_0).
     *
     *   Now, as long as we recorded all of the variables that occurred inside the aggregate, the intersection
     *   of these two sets (variables in an outer scope) and (variables occurring in t
     **/

    /**
     * A variable is considered *injected* into an aggregate if it occurs both within the aggregate AS WELL
     * AS *grounded* in the outer scope. An outer scope could be an outer aggregate, or also just the
     *base/rule scope. (i.e. not inside of another aggregate that is not an ancestor of this one).
     *
     *   So then in order to calculate the set of variables that have been injected into an aggregate, we
     * perform the following steps:
     *
     *   1. Find the set of variables occurring within the aggregate "aggregate"
     *   2. Find the set of variables that occur grounded in the outside scope
     *       2a. Replace all non-ancestral aggregates with variables so that we can ignore their variable
     * contents (it is not relevant). 2b. Replace the target aggregate with a variable as well 2c. Ground all
     * occurrences of the non-ancestral aggregate variable replacements (i.e. grounding_atom(+aggr_var_0)) 2d.
     * Visit all variables occurring in this tweaked clause that we made from steps 2a-2c, and check if they
     * are grounded, and also occur in the set of target aggregate variables. Then it is an injected variable.
     **/
    // Step 1
    std::set<std::string> variablesInTargetAggregate;
    visit(aggregate,
            [&](const Variable& variable) { variablesInTargetAggregate.insert(variable.getName()); });

    std::set<Own<Aggregator>> ancestorAggregates;

    visit(clause, [&](const Aggregator& ancestor) {
        visit(ancestor, [&](const Aggregator& agg) {
            if (agg == aggregate) {
                ancestorAggregates.insert(clone(ancestor));
            }
        });
    });

    // 1. Replace non-ancestral aggregates with variables
    struct ReplaceAggregatesWithVariables : public NodeMapper {
        // Variables introduced to replace aggregators
        mutable std::set<std::string> aggregatorVariables;
        // ancestors are never replaced so don't need to clone, but actually
        // it will have a child that will become invalid at one point. This is a concern. Need to clone these
        // bastards too.
        std::set<Own<Aggregator>> ancestors;
        // make sure you clone the agg first.
        Own<Aggregator> targetAggregate;

        const std::set<std::string>& getAggregatorVariables() {
            return aggregatorVariables;
        }

        ReplaceAggregatesWithVariables(std::set<Own<Aggregator>> ancestors, Own<Aggregator> targetAggregate)
                : ancestors(std::move(ancestors)), targetAggregate(std::move(targetAggregate)) {}

        std::unique_ptr<Node> operator()(std::unique_ptr<Node> node) const override {
            static int numReplaced = 0;
            if (auto* aggregate = as<Aggregator>(node)) {
                // If we come across an aggregate that is NOT an ancestor of
                // the target aggregate, or that IS itself the target aggregate,
                // we should replace it with a dummy variable.
                bool isAncestor = false;
                for (auto& ancestor : ancestors) {
                    if (*ancestor == *aggregate) {
                        isAncestor = true;
                        break;
                    }
                }
                if (!isAncestor || *aggregate == *targetAggregate) {
                    // Replace the aggregator with a variable
                    std::stringstream newVariableName;
                    newVariableName << "+aggr_var_" << numReplaced++;
                    // Keep track of which variables are bound to aggregators
                    // (only applicable to non-ancestral aggregates)
                    if (!isAncestor) {
                        // we don't want to ground the target aggregate
                        // (why?) because these variables could be injected
                        // and that is fine.
                        aggregatorVariables.insert(newVariableName.str());
                        // but we are not supposed to be judging the aggregate as grounded and injected into
                        // the aggregate, that wouldn't make sense
                    }
                    return mk<Variable>(newVariableName.str());
                }
            }
            node->apply(*this);
            return node;
        }
    };
    // 2. make a clone of the clause and then apply that mapper onto it
    auto clauseCopy = clone(clause);
    auto tweakedClause = mk<Clause>(QualifiedName::fromString("*"));
    tweakedClause->setBodyLiterals(clone(clause.getBodyLiterals()));

    // copy in the head as a negated atom
    tweakedClause->addToBody(mk<Negation>(clone(clause.getHead())));
    // copy in body literals and also add the old head as a negated atom
    ReplaceAggregatesWithVariables update(std::move(ancestorAggregates), clone(aggregate));
    tweakedClause->apply(update);
    // the update will now tell us which variables we need to ground!
    auto groundingAtom = mk<Atom>(QualifiedName::fromString("+grounding_atom"));
    for (const std::string& variableName : update.getAggregatorVariables()) {
        groundingAtom->addArgument(mk<Variable>(variableName));
    }
    // add the newly created grounding atom to the body
    tweakedClause->addToBody(std::move(groundingAtom));

    std::set<std::string> injectedVariables;
    // Search through the tweakedClause to find groundings!
    for (const auto& argPair : analysis::getGroundedTerms(tu, *tweakedClause)) {
        if (const auto* variable = as<Variable>(argPair.first)) {
            bool varIsGrounded = argPair.second;
            if (varIsGrounded && variablesInTargetAggregate.find(variable->getName()) !=
                                         variablesInTargetAggregate.end()) {
                // then we have found an injected variable, lovely!!
                injectedVariables.insert(variable->getName());
            }
        }
    }
    // Remove any variables that occur in the target expression of the aggregate
    if (aggregate.getTargetExpression() != nullptr) {
        visit(*aggregate.getTargetExpression(),
                [&](const Variable& v) { injectedVariables.erase(v.getName()); });
    }

    return injectedVariables;
}

}  // namespace souffle::ast::analysis
