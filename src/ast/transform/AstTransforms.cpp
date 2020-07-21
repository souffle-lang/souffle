/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTransforms.cpp
 *
 * Implementation of AST transformation passes.
 *
 ***********************************************************************/

#include "ast/transform/AstTransforms.h"
#include "AggregateOp.h"
#include "BinaryConstraintOps.h"
#include "ErrorReport.h"
#include "FunctorOps.h"
#include "GraphUtils.h"
#include "RamTypes.h"
#include "RelationTag.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstAttribute.h"
#include "ast/AstClause.h"
#include "ast/AstFunctorDeclaration.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/AstGroundAnalysis.h"
#include "ast/analysis/AstIOTypeAnalysis.h"
#include "ast/analysis/AstTypeAnalysis.h"
#include "ast/analysis/AstTypeEnvironmentAnalysis.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "utility/ContainerUtil.h"
#include "utility/FunctionalUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>

namespace souffle {

bool NullTransformer::transform(AstTranslationUnit&) {
    return false;
}

bool PipelineTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    for (auto& transformer : pipeline) {
        changed |= applySubtransformer(translationUnit, transformer.get());
    }
    return changed;
}

bool ConditionalTransformer::transform(AstTranslationUnit& translationUnit) {
    return condition() ? applySubtransformer(translationUnit, transformer.get()) : false;
}

bool WhileTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    while (condition()) {
        changed |= applySubtransformer(translationUnit, transformer.get());
    }
    return changed;
}

bool FixpointTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    while (applySubtransformer(translationUnit, transformer.get())) {
        changed = true;
    }
    return changed;
}

bool RemoveRelationCopiesTransformer::removeRelationCopies(AstTranslationUnit& translationUnit) {
    using alias_map = std::map<AstQualifiedName, AstQualifiedName>;

    // collect aliases
    alias_map isDirectAliasOf;

    auto* ioType = translationUnit.getAnalysis<IOType>();

    AstProgram& program = *translationUnit.getProgram();

    // search for relations only defined by a single rule ..
    for (AstRelation* rel : program.getRelations()) {
        const auto& clauses = getClauses(program, *rel);
        if (!ioType->isIO(rel) && clauses.size() == 1u) {
            // .. of shape r(x,y,..) :- s(x,y,..)
            AstClause* cl = clauses[0];
            std::vector<AstAtom*> bodyAtoms = getBodyLiterals<AstAtom>(*cl);
            if (!isFact(*cl) && cl->getBodyLiterals().size() == 1u && bodyAtoms.size() == 1u) {
                AstAtom* atom = bodyAtoms[0];
                if (equal_targets(cl->getHead()->getArguments(), atom->getArguments())) {
                    // Requirements:
                    // 1) (checked) It is a rule with exactly one body.
                    // 3) (checked) The body consists of an atom.
                    // 4) (checked) The atom's arguments must be identical to the rule's head.
                    // 5) (pending) The rules's head must consist only of either:
                    //  5a) Variables
                    //  5b) Records unpacked into variables
                    // 6) (pending) Each variable must have a distinct name.
                    bool onlyDistinctHeadVars = true;
                    std::set<std::string> headVars;

                    auto args = cl->getHead()->getArguments();
                    while (onlyDistinctHeadVars && !args.empty()) {
                        const auto cur = args.back();
                        args.pop_back();

                        if (auto var = dynamic_cast<const AstVariable*>(cur)) {
                            onlyDistinctHeadVars &= headVars.insert(var->getName()).second;
                        } else if (auto init = dynamic_cast<const AstRecordInit*>(cur)) {
                            // records are decomposed and their arguments are checked
                            for (auto rec_arg : init->getArguments()) {
                                args.push_back(rec_arg);
                            }
                        } else {
                            onlyDistinctHeadVars = false;
                        }
                    }

                    if (onlyDistinctHeadVars) {
                        // all arguments are either distinct variables or records unpacked into distinct
                        // variables
                        isDirectAliasOf[cl->getHead()->getQualifiedName()] = atom->getQualifiedName();
                    }
                }
            }
        }
    }

    // map each relation to its ultimate alias (could be transitive)
    alias_map isAliasOf;

    // track any copy cycles; cyclic rules are effectively empty
    std::set<AstQualifiedName> cycle_reps;

    for (std::pair<AstQualifiedName, AstQualifiedName> cur : isDirectAliasOf) {
        // compute replacement

        std::set<AstQualifiedName> visited;
        visited.insert(cur.first);
        visited.insert(cur.second);

        auto pos = isDirectAliasOf.find(cur.second);
        while (pos != isDirectAliasOf.end()) {
            if (visited.count(pos->second) != 0u) {
                cycle_reps.insert(cur.second);
                break;
            }
            cur.second = pos->second;
            pos = isDirectAliasOf.find(cur.second);
        }
        isAliasOf[cur.first] = cur.second;
    }

    if (isAliasOf.empty()) {
        return false;
    }

    // replace usage of relations according to alias map
    visitDepthFirst(program, [&](const AstAtom& atom) {
        auto pos = isAliasOf.find(atom.getQualifiedName());
        if (pos != isAliasOf.end()) {
            const_cast<AstAtom&>(atom).setQualifiedName(pos->second);
        }
    });

    // break remaining cycles
    for (const auto& rep : cycle_reps) {
        const auto& rel = *getRelation(program, rep);
        const auto& clauses = getClauses(program, rel);
        assert(clauses.size() == 1u && "unexpected number of clauses in relation");
        program.removeClause(clauses[0]);
    }

    // remove unused relations
    for (const auto& cur : isAliasOf) {
        if (cycle_reps.count(cur.first) == 0u) {
            program.removeRelation(getRelation(program, cur.first)->getQualifiedName());
        }
    }

    return true;
}

bool UniqueAggregationVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    // make variables in aggregates unique
    int aggNumber = 0;
    visitDepthFirstPostOrder(*translationUnit.getProgram(), [&](const AstAggregator& agg) {
        // only applicable for aggregates with target expression
        if (agg.getTargetExpression() == nullptr) {
            return;
        }

        // get all variables in the target expression
        std::set<std::string> names;
        visitDepthFirst(
                *agg.getTargetExpression(), [&](const AstVariable& var) { names.insert(var.getName()); });

        // rename them
        visitDepthFirst(agg, [&](const AstVariable& var) {
            auto pos = names.find(var.getName());
            if (pos == names.end()) {
                return;
            }
            const_cast<AstVariable&>(var).setName(" " + var.getName() + toString(aggNumber));
            changed = true;
        });

        // increment aggregation number
        aggNumber++;
    });
    return changed;
}

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
            if (!needsMaterializedRelation(agg)) {
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

bool MaterializeAggregationQueriesTransformer::needsMaterializedRelation(const AstAggregator& agg) {
    // everything with more than 1 body literal => materialize
    if (agg.getBodyLiterals().size() > 1) {
        return true;
    }

    // Inspect remaining atom more closely
    const AstAtom* atom = dynamic_cast<const AstAtom*>(agg.getBodyLiterals()[0]);
    if (atom == nullptr) {
        // No atoms, so materialize
        return true;
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

bool RemoveEmptyRelationsTransformer::removeEmptyRelations(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();
    auto* ioTypes = translationUnit.getAnalysis<IOType>();
    bool changed = false;
    for (auto rel : program.getRelations()) {
        if (!getClauses(program, *rel).empty() || ioTypes->isInput(rel)) {
            continue;
        }
        changed |= removeEmptyRelationUses(translationUnit, rel);

        bool usedInAggregate = false;
        visitDepthFirst(program, [&](const AstAggregator& agg) {
            for (const auto lit : agg.getBodyLiterals()) {
                visitDepthFirst(*lit, [&](const AstAtom& atom) {
                    if (getAtomRelation(&atom, &program) == rel) {
                        usedInAggregate = true;
                    }
                });
            }
        });

        if (!usedInAggregate && !ioTypes->isOutput(rel)) {
            program.removeRelation(rel->getQualifiedName());
            changed = true;
        }
    }
    return changed;
}

bool RemoveEmptyRelationsTransformer::removeEmptyRelationUses(
        AstTranslationUnit& translationUnit, AstRelation* emptyRelation) {
    AstProgram& program = *translationUnit.getProgram();
    bool changed = false;

    //
    // (1) drop rules from the program that have empty relations in their bodies.
    // (2) drop negations of empty relations
    //
    // get all clauses
    std::vector<const AstClause*> clauses;
    visitDepthFirst(program, [&](const AstClause& cur) { clauses.push_back(&cur); });

    // clean all clauses
    for (const AstClause* cl : clauses) {
        // check for an atom whose relation is the empty relation

        bool removed = false;
        for (AstLiteral* lit : cl->getBodyLiterals()) {
            if (auto* arg = dynamic_cast<AstAtom*>(lit)) {
                if (getAtomRelation(arg, &program) == emptyRelation) {
                    program.removeClause(cl);
                    removed = true;
                    changed = true;
                    break;
                }
            }
        }

        if (!removed) {
            // check whether a negation with empty relations exists

            bool rewrite = false;
            for (AstLiteral* lit : cl->getBodyLiterals()) {
                if (auto* neg = dynamic_cast<AstNegation*>(lit)) {
                    if (getAtomRelation(neg->getAtom(), &program) == emptyRelation) {
                        rewrite = true;
                        break;
                    }
                }
            }

            if (rewrite) {
                // clone clause without negation for empty relations

                auto res = std::unique_ptr<AstClause>(cloneHead(cl));

                for (AstLiteral* lit : cl->getBodyLiterals()) {
                    if (auto* neg = dynamic_cast<AstNegation*>(lit)) {
                        if (getAtomRelation(neg->getAtom(), &program) != emptyRelation) {
                            res->addToBody(souffle::clone(lit));
                        }
                    } else {
                        res->addToBody(souffle::clone(lit));
                    }
                }

                program.removeClause(cl);
                program.addClause(std::move(res));
                changed = true;
            }
        }
    }

    return changed;
}

bool RemoveRedundantRelationsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    auto* redundantRelationsAnalysis = translationUnit.getAnalysis<RedundantRelations>();
    const std::set<const AstRelation*>& redundantRelations =
            redundantRelationsAnalysis->getRedundantRelations();
    if (!redundantRelations.empty()) {
        for (auto rel : redundantRelations) {
            translationUnit.getProgram()->removeRelation(rel->getQualifiedName());
            changed = true;
        }
    }
    return changed;
}

bool RemoveBooleanConstraintsTransformer::transform(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();

    // If any boolean constraints exist, they will be removed
    bool changed = false;
    visitDepthFirst(program, [&](const AstBooleanConstraint&) { changed = true; });

    // Remove true and false constant literals from all aggregators
    struct removeBools : public AstNodeMapper {
        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Remove them from child nodes
            node->apply(*this);

            if (auto* aggr = dynamic_cast<AstAggregator*>(node.get())) {
                bool containsTrue = false;
                bool containsFalse = false;

                // Check if aggregator body contains booleans.
                for (AstLiteral* lit : aggr->getBodyLiterals()) {
                    if (auto* bc = dynamic_cast<AstBooleanConstraint*>(lit)) {
                        if (bc->isTrue()) {
                            containsTrue = true;
                        } else {
                            containsFalse = true;
                        }
                    }
                }

                // Only keep literals that aren't boolean constraints
                if (containsFalse || containsTrue) {
                    auto replacementAggregator = souffle::clone(aggr);
                    std::vector<std::unique_ptr<AstLiteral>> newBody;

                    bool isEmpty = true;

                    // Don't bother copying over body literals if any are false
                    if (!containsFalse) {
                        for (AstLiteral* lit : aggr->getBodyLiterals()) {
                            // Don't add in boolean constraints
                            if (dynamic_cast<AstBooleanConstraint*>(lit) == nullptr) {
                                isEmpty = false;
                                newBody.push_back(souffle::clone(lit));
                            }
                        }

                        // If the body is still empty and the original body contains true add it now.
                        if (containsTrue && isEmpty) {
                            newBody.push_back(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                                    std::make_unique<AstNumericConstant>(1),
                                    std::make_unique<AstNumericConstant>(1)));

                            isEmpty = false;
                        }
                    }

                    if (containsFalse || isEmpty) {
                        // Empty aggregator body!
                        // Not currently handled, so add in a false literal in the body
                        // E.g. max x : { } =becomes=> max 1 : {0 = 1}
                        newBody.push_back(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                                std::make_unique<AstNumericConstant>(0),
                                std::make_unique<AstNumericConstant>(1)));
                    }

                    replacementAggregator->setBody(std::move(newBody));
                    return replacementAggregator;
                }
            }

            // no false or true, so return the original node
            return node;
        }
    };

    removeBools update;
    program.apply(update);

    // Remove true and false constant literals from all clauses
    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            bool containsTrue = false;
            bool containsFalse = false;

            for (AstLiteral* lit : clause->getBodyLiterals()) {
                if (auto* bc = dynamic_cast<AstBooleanConstraint*>(lit)) {
                    bc->isTrue() ? containsTrue = true : containsFalse = true;
                }
            }

            if (containsFalse) {
                // Clause will always fail
                program.removeClause(clause);
            } else if (containsTrue) {
                auto replacementClause = std::unique_ptr<AstClause>(cloneHead(clause));

                // Only keep non-'true' literals
                for (AstLiteral* lit : clause->getBodyLiterals()) {
                    if (dynamic_cast<AstBooleanConstraint*>(lit) == nullptr) {
                        replacementClause->addToBody(souffle::clone(lit));
                    }
                }

                program.removeClause(clause);
                program.addClause(std::move(replacementClause));
            }
        }
    }

    return changed;
}

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

bool ReplaceSingletonVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    AstProgram& program = *translationUnit.getProgram();

    // Node-mapper to replace a set of singletons with unnamed variables
    struct replaceSingletons : public AstNodeMapper {
        std::set<std::string>& singletons;

        replaceSingletons(std::set<std::string>& singletons) : singletons(singletons) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* var = dynamic_cast<AstVariable*>(node.get())) {
                if (singletons.find(var->getName()) != singletons.end()) {
                    return std::make_unique<AstUnnamedVariable>();
                }
            }
            node->apply(*this);
            return node;
        }
    };

    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            std::set<std::string> nonsingletons;
            std::set<std::string> vars;

            visitDepthFirst(*clause, [&](const AstVariable& var) {
                const std::string& name = var.getName();
                if (vars.find(name) != vars.end()) {
                    // Variable seen before, so not a singleton variable
                    nonsingletons.insert(name);
                } else {
                    vars.insert(name);
                }
            });

            std::set<std::string> ignoredVars;

            // Don't unname singleton variables occurring in records.
            // TODO (azreika): remove this check once issue #420 is fixed
            std::set<std::string> recordVars;
            visitDepthFirst(*clause, [&](const AstRecordInit& rec) {
                visitDepthFirst(rec, [&](const AstVariable& var) { ignoredVars.insert(var.getName()); });
            });

            // Don't unname singleton variables occuring in constraints.
            std::set<std::string> constraintVars;
            visitDepthFirst(*clause, [&](const AstConstraint& cons) {
                visitDepthFirst(cons, [&](const AstVariable& var) { ignoredVars.insert(var.getName()); });
            });

            std::set<std::string> singletons;
            for (auto& var : vars) {
                if ((nonsingletons.find(var) == nonsingletons.end()) &&
                        (ignoredVars.find(var) == ignoredVars.end())) {
                    changed = true;
                    singletons.insert(var);
                }
            }

            // Replace the singletons found with underscores
            replaceSingletons update(singletons);
            clause->apply(update);
        }
    }

    return changed;
}

bool NameUnnamedVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    static constexpr const char* boundPrefix = "+underscore";
    static size_t underscoreCount = 0;

    struct nameVariables : public AstNodeMapper {
        mutable bool changed = false;
        nameVariables() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (dynamic_cast<AstUnnamedVariable*>(node.get()) != nullptr) {
                changed = true;
                std::stringstream name;
                name << boundPrefix << "_" << underscoreCount++;
                return std::make_unique<AstVariable>(name.str());
            }
            node->apply(*this);
            return node;
        }
    };

    AstProgram& program = *translationUnit.getProgram();
    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            nameVariables update;
            clause->apply(update);
            changed |= update.changed;
        }
    }

    return changed;
}

bool RemoveRedundantSumsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct ReplaceSumWithCount : public AstNodeMapper {
        ReplaceSumWithCount() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Apply to all aggregates of the form
            // sum k : { .. } where k is a constant
            if (auto* agg = dynamic_cast<AstAggregator*>(node.get())) {
                if (agg->getOperator() == AggregateOp::SUM) {
                    if (const auto* constant =
                                    dynamic_cast<const AstNumericConstant*>(agg->getTargetExpression())) {
                        changed = true;
                        // Then construct the new thing to replace it with
                        auto count = std::make_unique<AstAggregator>(AggregateOp::COUNT);
                        // Duplicate the body of the aggregate
                        std::vector<std::unique_ptr<AstLiteral>> newBody;
                        for (const auto& lit : agg->getBodyLiterals()) {
                            newBody.push_back(souffle::clone(lit));
                        }
                        count->setBody(std::move(newBody));
                        auto number = souffle::clone(constant);
                        // Now it's constant * count : { ... }
                        auto result = std::make_unique<AstIntrinsicFunctor>(
                                "*", std::move(number), std::move(count));

                        return result;
                    }
                }
            }
            node->apply(*this);
            return node;
        }

        // variables
        mutable bool changed = false;
    };

    ReplaceSumWithCount update;
    translationUnit.getProgram()->apply(update);
    return update.changed;
}

bool NormaliseConstraintsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    // set a prefix for variables bound by magic-set for identification later
    // prepended by + to avoid conflict with user-defined variables
    static constexpr const char* boundPrefix = "+abdul";

    AstProgram& program = *translationUnit.getProgram();

    /* Create a node mapper that recursively replaces all constants and underscores
     * with named variables.
     *
     * The mapper keeps track of constraints that should be added to the original
     * clause it is being applied on in a given constraint set.
     */
    struct constraintNormaliser : public AstNodeMapper {
        std::set<AstBinaryConstraint*>& constraints;
        mutable int changeCount;

        constraintNormaliser(std::set<AstBinaryConstraint*>& constraints, int changeCount)
                : constraints(constraints), changeCount(changeCount) {}

        bool hasChanged() const {
            return changeCount > 0;
        }

        int getChangeCount() const {
            return changeCount;
        }

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* stringConstant = dynamic_cast<AstStringConstant*>(node.get())) {
                // string constant found
                changeCount++;

                // create new variable name (with appropriate suffix)
                std::string constantValue = stringConstant->getConstant();
                std::stringstream newVariableName;
                newVariableName << boundPrefix << changeCount << "_" << constantValue << "_s";

                // create new constraint (+abdulX = constant)
                auto newVariable = std::make_unique<AstVariable>(newVariableName.str());
                constraints.insert(new AstBinaryConstraint(
                        BinaryConstraintOp::EQ, souffle::clone(newVariable), souffle::clone(stringConstant)));

                // update constant to be the variable created
                return newVariable;
            } else if (auto* numberConstant = dynamic_cast<AstNumericConstant*>(node.get())) {
                // number constant found
                changeCount++;

                // create new variable name (with appropriate suffix)
                std::stringstream newVariableName;
                newVariableName << boundPrefix << changeCount << "_" << numberConstant->getConstant() << "_n";

                assert(numberConstant->getType() && "numeric constant hasn't been poly-constrained");
                auto opEq = *numberConstant->getType() == AstNumericConstant::Type::Float
                                    ? BinaryConstraintOp::FEQ
                                    : BinaryConstraintOp::EQ;

                // create new constraint (+abdulX = constant)
                auto newVariable = std::make_unique<AstVariable>(newVariableName.str());
                constraints.insert(new AstBinaryConstraint(
                        opEq, souffle::clone(newVariable), souffle::clone(numberConstant)));

                // update constant to be the variable created
                return newVariable;
            } else if (dynamic_cast<AstUnnamedVariable*>(node.get()) != nullptr) {
                // underscore found
                changeCount++;

                // create new variable name
                std::stringstream newVariableName;
                newVariableName << "+underscore" << changeCount;

                return std::make_unique<AstVariable>(newVariableName.str());
            }

            node->apply(*this);
            return node;
        }
    };

    int changeCount = 0;  // number of constants and underscores seen so far

    // apply the change to all clauses in the program
    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            if (isFact(*clause)) {
                continue;  // don't normalise facts
            }

            std::set<AstBinaryConstraint*> constraints;
            constraintNormaliser update(constraints, changeCount);
            clause->apply(update);

            changeCount = update.getChangeCount();
            changed = changed || update.hasChanged();

            for (AstBinaryConstraint* constraint : constraints) {
                clause->addToBody(std::unique_ptr<AstBinaryConstraint>(constraint));
            }
        }
    }

    return changed;
}

bool RemoveTypecastsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct TypecastRemover : public AstNodeMapper {
        mutable bool changed{false};

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // remove sub-typecasts first
            node->apply(*this);

            // if current node is a typecast, replace with the value directly
            if (auto* cast = dynamic_cast<AstTypeCast*>(node.get())) {
                changed = true;
                return souffle::clone(cast->getValue());
            }

            // otherwise, return the original node
            return node;
        }
    };

    TypecastRemover update;
    translationUnit.getProgram()->apply(update);

    return update.changed;
}

bool PolymorphicObjectsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct TypeRewriter : public AstNodeMapper {
        mutable bool changed{false};
        const TypeAnalysis& typeAnalysis;
        ErrorReport& report;

        TypeRewriter(const TypeAnalysis& typeAnalysis, ErrorReport& report)
                : typeAnalysis(typeAnalysis), report(report) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Utility lambdas to determine if all args are of the same type.
            auto isFloat = [&](const AstArgument* argument) {
                return isOfKind(typeAnalysis.getTypes(argument), TypeAttribute::Float);
            };
            auto isUnsigned = [&](const AstArgument* argument) {
                return isOfKind(typeAnalysis.getTypes(argument), TypeAttribute::Unsigned);
            };
            auto isSymbol = [&](const AstArgument* argument) {
                return isOfKind(typeAnalysis.getTypes(argument), TypeAttribute::Symbol);
            };

            // rewrite sub-expressions first
            node->apply(*this);

            // It's possible that at this stage we get an undeclared clause.
            // In this case types can't be assigned to it, and the procedure getTypes can fail
            try {
                // Handle numeric constant
                if (auto* numericConstant = dynamic_cast<AstNumericConstant*>(node.get())) {
                    // Check if there is no value yet.
                    if (!numericConstant->getType().has_value()) {
                        TypeSet types = typeAnalysis.getTypes(numericConstant);

                        auto hasOfKind = [&](TypeAttribute kind) -> bool {
                            return any_of(types, [&](const Type& type) { return isOfKind(type, kind); });
                        };
                        if (hasOfKind(TypeAttribute::Signed)) {
                            numericConstant->setType(AstNumericConstant::Type::Int);
                            changed = true;
                        } else if (hasOfKind(TypeAttribute::Unsigned)) {
                            numericConstant->setType(AstNumericConstant::Type::Uint);
                            changed = true;
                        } else if (hasOfKind(TypeAttribute::Float)) {
                            numericConstant->setType(AstNumericConstant::Type::Float);
                            changed = true;
                        }
                    }
                }

                // Handle functor
                auto* functor = as<AstIntrinsicFunctor>(node);
                if (functor && !functor->getFunctionInfo()) {
                    // any valid candidate will do. pick the first.
                    auto candidates = validOverloads(typeAnalysis, *functor);
                    if (!candidates.empty()) {
                        functor->setFunctionInfo(candidates.front().get());
                        changed = true;
                    }
                }

                // Handle binary constraint
                if (auto* binaryConstraint = dynamic_cast<AstBinaryConstraint*>(node.get())) {
                    if (isOverloaded(binaryConstraint->getOperator())) {
                        // Get arguments
                        auto* leftArg = binaryConstraint->getLHS();
                        auto* rightArg = binaryConstraint->getRHS();
                        auto oldOp = binaryConstraint->getOperator();

                        // Both args must be of the same type
                        if (isFloat(leftArg) && isFloat(rightArg)) {
                            binaryConstraint->setOperator(convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Float));
                        } else if (isUnsigned(leftArg) && isUnsigned(rightArg)) {
                            binaryConstraint->setOperator(convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Unsigned));
                        } else if (isSymbol(leftArg) && isSymbol(rightArg)) {
                            binaryConstraint->setOperator(convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Symbol));
                        }

                        changed |= binaryConstraint->getOperator() != oldOp;
                    }
                }

                if (auto* aggregator = dynamic_cast<AstAggregator*>(node.get())) {
                    if (isOverloadedAggregator(aggregator->getOperator())) {
                        auto* targetExpression = aggregator->getTargetExpression();
                        auto oldOp = aggregator->getOperator();

                        if (isFloat(targetExpression)) {
                            aggregator->setOperator(convertOverloadedAggregator(
                                    aggregator->getOperator(), TypeAttribute::Float));
                        } else if (isUnsigned(targetExpression)) {
                            aggregator->setOperator(convertOverloadedAggregator(
                                    aggregator->getOperator(), TypeAttribute::Unsigned));
                        }

                        changed |= aggregator->getOperator() != oldOp;
                    }
                }
            } catch (std::out_of_range&) {
                // No types to convert in undeclared clauses
            }

            return node;
        }
    };
    const TypeAnalysis& typeAnalysis = *translationUnit.getAnalysis<TypeAnalysis>();
    TypeRewriter update(typeAnalysis, translationUnit.getErrorReport());
    translationUnit.getProgram()->apply(update);
    return update.changed;
}

bool AstUserDefinedFunctorsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct UserFunctorRewriter : public AstNodeMapper {
        mutable bool changed{false};
        const AstProgram& program;
        ErrorReport& report;

        UserFunctorRewriter(const AstProgram& program, ErrorReport& report)
                : program(program), report(report){};

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            node->apply(*this);

            if (auto* userFunctor = dynamic_cast<AstUserDefinedFunctor*>(node.get())) {
                const AstFunctorDeclaration* functorDeclaration =
                        getFunctorDeclaration(program, userFunctor->getName());

                // Check if the functor has been declared
                if (functorDeclaration == nullptr) {
                    report.addError("User-defined functor hasn't been declared", userFunctor->getSrcLoc());
                    return node;
                }

                // Check arity correctness.
                if (functorDeclaration->getArity() != userFunctor->getArguments().size()) {
                    report.addError("Mismatching number of arguments of functor", userFunctor->getSrcLoc());
                    return node;
                }

                // Set types of functor instance based on its declaration.
                userFunctor->setTypes(
                        functorDeclaration->getArgsTypes(), functorDeclaration->getReturnType());

                changed = true;
            }
            return node;
        }
    };
    UserFunctorRewriter update(*translationUnit.getProgram(), translationUnit.getErrorReport());
    translationUnit.getProgram()->apply(update);
    return update.changed;
}

bool FoldAnonymousRecords::isValidRecordConstraint(const AstLiteral* literal) {
    auto constraint = dynamic_cast<const AstBinaryConstraint*>(literal);

    if (constraint == nullptr) {
        return false;
    }

    const auto* left = constraint->getLHS();
    const auto* right = constraint->getRHS();

    const auto* leftRecord = dynamic_cast<const AstRecordInit*>(left);
    const auto* rightRecord = dynamic_cast<const AstRecordInit*>(right);

    // Check if arguments are records records.
    if ((leftRecord == nullptr) || (rightRecord == nullptr)) {
        return false;
    }

    // Check if records are of the same size.
    if (leftRecord->getChildNodes().size() != rightRecord->getChildNodes().size()) {
        return false;
    }

    // Check if operator is "=" or "!="
    auto op = constraint->getOperator();

    return isEqConstraint(op) || isEqConstraint(negatedConstraintOp(op));
}

bool FoldAnonymousRecords::containsValidRecordConstraint(const AstClause& clause) {
    bool contains = false;
    visitDepthFirst(clause, [&](const AstBinaryConstraint& binary) {
        contains = (contains || isValidRecordConstraint(&binary));
    });
    return contains;
}

std::vector<std::unique_ptr<AstLiteral>> FoldAnonymousRecords::expandRecordBinaryConstraint(
        const AstBinaryConstraint& constraint) {
    std::vector<std::unique_ptr<AstLiteral>> replacedContraint;

    const auto* left = dynamic_cast<AstRecordInit*>(constraint.getLHS());
    const auto* right = dynamic_cast<AstRecordInit*>(constraint.getRHS());
    assert(left != nullptr && "Non-record passed to record method");
    assert(right != nullptr && "Non-record passed to record method");

    auto leftChildren = left->getArguments();
    auto rightChildren = right->getArguments();

    assert(leftChildren.size() == rightChildren.size());

    // [a, b..] = [c, d...] → a = c, b = d ...
    for (size_t i = 0; i < leftChildren.size(); ++i) {
        auto newConstraint = std::make_unique<AstBinaryConstraint>(
                constraint.getOperator(), souffle::clone(leftChildren[i]), souffle::clone(rightChildren[i]));
        replacedContraint.push_back(std::move(newConstraint));
    }

    // Handle edge case. Empty records.
    if (leftChildren.size() == 0) {
        if (isEqConstraint(constraint.getOperator())) {
            replacedContraint.emplace_back(new AstBooleanConstraint(true));
        } else {
            replacedContraint.emplace_back(new AstBooleanConstraint(false));
        }
    }

    return replacedContraint;
}

void FoldAnonymousRecords::transformClause(
        const AstClause& clause, std::vector<std::unique_ptr<AstClause>>& newClauses) {
    // If we have an inequality constraint, we need to create new clauses
    // At most one inequality constraint will be expanded in a single pass.
    AstBinaryConstraint* neqConstraint = nullptr;

    std::vector<std::unique_ptr<AstLiteral>> newBody;
    for (auto* literal : clause.getBodyLiterals()) {
        if (isValidRecordConstraint(literal)) {
            const AstBinaryConstraint& constraint = dynamic_cast<AstBinaryConstraint&>(*literal);

            // Simple case, [a_0, ..., a_n] = [b_0, ..., b_n]
            if (isEqConstraint(constraint.getOperator())) {
                auto transformedLiterals = expandRecordBinaryConstraint(constraint);
                std::move(std::begin(transformedLiterals), std::end(transformedLiterals),
                        std::back_inserter(newBody));

                // else if: Case [a_0, ..., a_n] != [b_0, ..., b_n].
                // track single such case, it will be expanded in the end.
            } else if (neqConstraint == nullptr) {
                neqConstraint = dynamic_cast<AstBinaryConstraint*>(literal);

                // Else: repeated inequality.
            } else {
                newBody.push_back(souffle::clone(literal));
            }

            // else, we simply copy the literal.
        } else {
            newBody.push_back(souffle::clone(literal));
        }
    }

    // If no inequality: create a single modified clause.
    if (neqConstraint == nullptr) {
        auto newClause = souffle::clone(&clause);
        newClause->setBodyLiterals(std::move(newBody));
        newClauses.emplace_back(std::move(newClause));

        // Else: For each pair in negation, we need an extra clause.
    } else {
        auto transformedLiterals = expandRecordBinaryConstraint(*neqConstraint);

        for (auto it = begin(transformedLiterals); it != end(transformedLiterals); ++it) {
            auto newClause = souffle::clone(&clause);
            auto copyBody = souffle::clone(newBody);
            copyBody.push_back(std::move(*it));

            newClause->setBodyLiterals(std::move(copyBody));

            newClauses.push_back(std::move(newClause));
        }
    }
}

bool FoldAnonymousRecords::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    AstProgram& program = *translationUnit.getProgram();

    std::vector<std::unique_ptr<AstClause>> newClauses;

    for (const auto* clause : program.getClauses()) {
        if (containsValidRecordConstraint(*clause)) {
            changed = true;
            transformClause(*clause, newClauses);
        } else {
            newClauses.emplace_back(clause->clone());
        }
    }

    // Update AstProgram.
    if (changed) {
        program.setClauses(std::move(newClauses));
    }
    return changed;
}

std::map<std::string, const AstRecordInit*> ResolveAnonymousRecordsAliases::findVariablesRecordMapping(
        AstTranslationUnit& tu, const AstClause& clause) {
    std::map<std::string, const AstRecordInit*> variableRecordMap;

    auto isVariable = [](AstNode* node) -> bool { return dynamic_cast<AstVariable*>(node) != nullptr; };
    auto isRecord = [](AstNode* node) -> bool { return dynamic_cast<AstRecordInit*>(node) != nullptr; };

    auto& typeAnalysis = *tu.getAnalysis<TypeAnalysis>();
    auto groundedTerms = getGroundedTerms(tu, clause);

    for (auto* literal : clause.getBodyLiterals()) {
        if (auto constraint = dynamic_cast<AstBinaryConstraint*>(literal)) {
            if (!isEqConstraint(constraint->getOperator())) {
                continue;
            }

            auto left = constraint->getLHS();
            auto right = constraint->getRHS();

            if (!isVariable(left) && !isVariable(right)) {
                continue;
            }

            if (!isRecord(left) && !isRecord(right)) {
                continue;
            }

            // TODO (darth_tytus): This should change in the future.
            // Currently type system assigns to anonymous records {- All types - }
            // which is inelegant.
            if (!typeAnalysis.getTypes(left).isAll()) {
                continue;
            }

            auto* variable = static_cast<AstVariable*>(isVariable(left) ? left : right);
            const auto& variableName = variable->getName();

            if (!groundedTerms.find(variable)->second) {
                continue;
            }

            // We are interested only in the first mapping.
            if (variableRecordMap.find(variableName) != variableRecordMap.end()) {
                continue;
            }

            auto* record = static_cast<AstRecordInit*>(isRecord(left) ? left : right);

            variableRecordMap.insert({variableName, record});
        }
    }

    return variableRecordMap;
}

bool ResolveAnonymousRecordsAliases::replaceNamedVariables(AstTranslationUnit& tu, AstClause& clause) {
    struct ReplaceVariables : public AstNodeMapper {
        std::map<std::string, const AstRecordInit*> varToRecordMap;

        ReplaceVariables(std::map<std::string, const AstRecordInit*> varToRecordMap)
                : varToRecordMap(std::move(varToRecordMap)){};

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto variable = dynamic_cast<AstVariable*>(node.get())) {
                auto iteratorToRecord = varToRecordMap.find(variable->getName());
                if (iteratorToRecord != varToRecordMap.end()) {
                    return souffle::clone(iteratorToRecord->second);
                }
            }

            node->apply(*this);

            return node;
        }
    };

    auto variableToRecordMap = findVariablesRecordMapping(tu, clause);
    bool changed = variableToRecordMap.size() > 0;
    if (changed) {
        ReplaceVariables update(std::move(variableToRecordMap));
        clause.apply(update);
    }
    return changed;
}

bool ResolveAnonymousRecordsAliases::replaceUnnamedVariable(AstClause& clause) {
    struct ReplaceUnnamed : public AstNodeMapper {
        mutable bool changed{false};
        ReplaceUnnamed() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            auto isUnnamed = [](AstNode* node) -> bool {
                return dynamic_cast<AstUnnamedVariable*>(node) != nullptr;
            };
            auto isRecord = [](AstNode* node) -> bool {
                return dynamic_cast<AstRecordInit*>(node) != nullptr;
            };

            if (auto constraint = dynamic_cast<AstBinaryConstraint*>(node.get())) {
                auto left = constraint->getLHS();
                auto right = constraint->getRHS();
                bool hasUnnamed = isUnnamed(left) || isUnnamed(right);
                bool hasRecord = isRecord(left) || isRecord(right);
                auto op = constraint->getOperator();
                if (hasUnnamed && hasRecord && isEqConstraint(op)) {
                    return std::make_unique<AstBooleanConstraint>(true);
                }
            }

            node->apply(*this);

            return node;
        }
    };

    ReplaceUnnamed update;
    clause.apply(update);

    return update.changed;
}

bool ResolveAnonymousRecordsAliases::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    for (auto* clause : translationUnit.getProgram()->getClauses()) {
        changed |= replaceNamedVariables(translationUnit, *clause);
        changed |= replaceUnnamedVariable(*clause);
    }

    return changed;
}

}  // end of namespace souffle
