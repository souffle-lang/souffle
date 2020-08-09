/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
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
#include "RamTypes.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/Clause.h"
#include "ast/LambdaNodeMapper.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Utils.h"
#include "ast/Variable.h"
#include "ast/Visitor.h"
#include "ast/analysis/Ground.h"
#include "ast/analysis/Type.h"
#include "ast/analysis/TypeEnvironment.h"
#include "ast/analysis/TypeSystem.h"
#include "parser/AggregateOp.h"
#include "utility/MiscUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

bool MaterializeAggregationQueriesTransformer::materializeAggregationQueries(
        AstTranslationUnit& translationUnit) {
    bool changed = false;

    AstProgram& program = *translationUnit.getProgram();
    const TypeEnvironment& env = translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    // if an aggregator has a body consisting of more than an atom => create new relation
    int counter = 0;
    visitDepthFirst(program, [&](const AstClause& clause) {
        visitDepthFirst(clause, [&](const AstAggregator& agg) {
            // check whether a materialization is required
            if (!needsMaterializedRelation(const_cast<AstAggregator&>(agg), program)) {
                return;
            }
            changed = true;

            // -- create a new clause --

            auto relName = "__agg_body_rel_" + toString(counter++);
            while (getRelation(program, relName) != nullptr) {
                relName = "__agg_body_rel_" + toString(counter++);
            }
            // create the new clause for the materialised rule
            auto* aggClause = new AstClause();
            // create the body of the new materialised rule
            for (const auto& cur : agg.getBodyLiterals()) {
                aggClause->addToBody(souffle::clone(cur));
            }
            // find stuff for which we need a grounding
            for (const auto& argPair : getGroundedTerms(translationUnit, *aggClause)) {
                const auto* variable = dynamic_cast<const AstVariable*>(argPair.first);
                bool variableIsGrounded = argPair.second;
                // if it's not even a variable type or the term is grounded
                // then skip it
                if (variable == nullptr || variableIsGrounded) {
                    continue;
                }

                for (const auto& lit : clause.getBodyLiterals()) {
                    const auto* atom = dynamic_cast<const AstAtom*>(lit);
                    if (atom == nullptr) {
                        continue;  // it's not an atom so it can't help ground anything
                    }
                    for (const auto& arg : atom->getArguments()) {
                        const auto* atomVariable = dynamic_cast<const AstVariable*>(arg);
                        // if this atom contains the variable I need to ground, add it
                        if ((atomVariable != nullptr) && variable->getName() == atomVariable->getName()) {
                            // expand the body with this one so that it will ground this variable
                            aggClause->addToBody(souffle::clone(atom));
                            break;
                        }
                    }
                }
            }

            auto* head = new AstAtom();
            head->setQualifiedName(relName);
            std::vector<bool> symbolArguments;

            // Ensure each variable is only added once
            std::set<std::string> variables;
            visitDepthFirst(*aggClause, [&](const AstVariable& var) { variables.insert(var.getName()); });

            // Insert all variables occurring in the body of the aggregate into the head
            for (const auto& var : variables) {
                head->addArgument(std::make_unique<AstVariable>(var));
            }

            aggClause->setHead(std::unique_ptr<AstAtom>(head));

            // instantiate unnamed variables in count operations
            if (agg.getOperator() == AggregateOp::COUNT) {
                int count = 0;
                for (const auto& cur : aggClause->getBodyLiterals()) {
                    cur->apply(makeLambdaAstMapper(
                            [&](std::unique_ptr<AstNode> node) -> std::unique_ptr<AstNode> {
                                // check whether it is a unnamed variable
                                auto* var = dynamic_cast<AstUnnamedVariable*>(node.get());
                                if (var == nullptr) {
                                    return node;
                                }

                                // replace by variable
                                auto name = " _" + toString(count++);
                                auto res = new AstVariable(name);

                                // extend head
                                head->addArgument(souffle::clone(res));

                                // return replacement
                                return std::unique_ptr<AstNode>(res);
                            }));
                }
            }

            // -- build relation --

            auto* rel = new AstRelation();
            rel->setQualifiedName(relName);
            // add attributes
            std::map<const AstArgument*, TypeSet> argTypes =
                    TypeAnalysis::analyseTypes(env, *aggClause, program);
            for (const auto& cur : head->getArguments()) {
                rel->addAttribute(std::make_unique<AstAttribute>(toString(*cur),
                        (isOfKind(argTypes[cur], TypeAttribute::Signed)) ? "number" : "symbol"));
            }

            program.addClause(std::unique_ptr<AstClause>(aggClause));
            program.addRelation(std::unique_ptr<AstRelation>(rel));

            // -- update aggregate --

            // count the usage of variables in the clause
            // outside of aggregates. Note that the visitor
            // is exhaustive hence double counting occurs
            // which needs to be deducted for variables inside
            // the aggregators and variables in the expression
            // of aggregate need to be added. Counter is zero
            // if the variable is local to the aggregate
            std::map<std::string, int> varCtr;
            visitDepthFirst(clause, [&](const AstArgument& arg) {
                if (const auto* a = dynamic_cast<const AstAggregator*>(&arg)) {
                    visitDepthFirst(arg, [&](const AstVariable& var) { varCtr[var.getName()]--; });
                    if (a->getTargetExpression() != nullptr) {
                        visitDepthFirst(*a->getTargetExpression(),
                                [&](const AstVariable& var) { varCtr[var.getName()]++; });
                    }
                } else {
                    visitDepthFirst(arg, [&](const AstVariable& var) { varCtr[var.getName()]++; });
                }
            });
            std::vector<std::unique_ptr<AstArgument>> args;
            for (auto arg : head->getArguments()) {
                if (auto* var = dynamic_cast<AstVariable*>(arg)) {
                    // replace local variable by underscore if local
                    if (varCtr[var->getName()] == 0) {
                        args.emplace_back(new AstUnnamedVariable());
                        continue;
                    }
                }
                args.emplace_back(arg->clone());
            }
            auto aggAtom =
                    std::make_unique<AstAtom>(head->getQualifiedName(), std::move(args), head->getSrcLoc());

            std::vector<std::unique_ptr<AstLiteral>> newBody;
            newBody.push_back(std::move(aggAtom));
            const_cast<AstAggregator&>(agg).setBody(std::move(newBody));
        });
    });
    return changed;
}

bool MaterializeAggregationQueriesTransformer::needsMaterializedRelation(
        AstAggregator& agg, AstProgram& program) {
    // everything with more than 1 body literal => materialize
    int countAtoms = 0;
    const AstAtom* atom = nullptr;
    for (const auto& literal : agg.getBodyLiterals()) {
        const AstAtom* currentAtom = dynamic_cast<const AstAtom*>(literal);
        if (currentAtom != nullptr) {
            ++countAtoms;
            atom = currentAtom;
        }
    }

    if (countAtoms > 1) {
        return true;
    }

    auto newAtom = std::make_unique<AstAtom>();
    // Create a dummy true atom so that an aggregate without any atom
    // can be evaluated without materialising
    if (atom == nullptr) {
        std::string relName = "+Tautology";
        newAtom->setQualifiedName(relName);
        auto clause = std::make_unique<AstClause>();
        clause->setHead(souffle::clone(newAtom));
        program.addClause(std::move(clause));
        if (getRelation(program, relName) == nullptr) {
            auto tautologyRel = std::make_unique<AstRelation>();
            tautologyRel->setQualifiedName(relName);
            program.addRelation(std::move(tautologyRel));
        }
        VecOwn<AstLiteral> newBody;
        for (const auto& lit : agg.getBodyLiterals()) {
            newBody.push_back(souffle::clone(lit));
        }
        newBody.push_back(souffle::clone(newAtom));
        agg.setBody(std::move(newBody));
        atom = newAtom.get();
    }

    // If the same variable occurs several times => materialize
    bool duplicates = false;
    std::set<std::string> vars;
    visitDepthFirst(*atom,
            [&](const AstVariable& var) { duplicates = duplicates || !vars.insert(var.getName()).second; });

    // If there are duplicates a materialization is required
    // for all others the materialization can be skipped
    return duplicates;
}

}  // end of namespace souffle
