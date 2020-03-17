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

#include "AstTransforms.h"
#include "AggregateOp.h"
#include "AstArgument.h"
#include "AstAttribute.h"
#include "AstClause.h"
#include "AstGroundAnalysis.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "AstProgram.h"
#include "AstQualifiedName.h"
#include "AstRelation.h"
#include "AstTypeAnalysis.h"
#include "AstTypeEnvironmentAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "FunctorOps.h"
#include "GraphUtils.h"
#include "PrecedenceGraph.h"
#include "RamTypes.h"
#include "TypeSystem.h"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>

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

            auto relName = "__agg_rel_" + toString(counter++);
            while (getRelation(program, relName) != nullptr) {
                relName = "__agg_rel_" + toString(counter++);
            }
            // create the new clause for the materialised thing
            auto* aggClause = new AstClause();
            // create the body of the new thing
            for (const auto& cur : agg.getBodyLiterals()) {
                aggClause->addToBody(std::unique_ptr<AstLiteral>(cur->clone()));
            }
            // find stuff for which we need a grounding
            for (const auto& argPair : getGroundedTerms(*aggClause)) {
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
                        continue;  // ignore these because they can't ground the variable
                    }
                    for (const auto& arg : atom->getArguments()) {
                        const auto* atomVariable = dynamic_cast<const AstVariable*>(arg);
                        // if this atom contains the variable I need to ground, add it
                        if ((atomVariable != nullptr) && variable->getName() == atomVariable->getName()) {
                            // expand the body with this one so that it will ground this variable
                            aggClause->addToBody(std::unique_ptr<AstLiteral>(atom->clone()));
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
            if (agg.getOperator() == AggregateOp::count) {
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
                                head->addArgument(std::unique_ptr<AstArgument>(res->clone()));

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
                    TypeAnalysis::analyseTypes(env, *aggClause, &program);
            for (const auto& cur : head->getArguments()) {
                rel->addAttribute(std::make_unique<AstAttribute>(
                        toString(*cur), (isNumberType(argTypes[cur])) ? "number" : "symbol"));
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
            auto* aggAtom = new AstAtom(head->getQualifiedName(), std::move(args), head->getSrcLoc());
            const_cast<AstAggregator&>(agg).clearBodyLiterals();
            const_cast<AstAggregator&>(agg).addBodyLiteral(std::unique_ptr<AstLiteral>(aggAtom));
        });
    });
    return changed;
}

bool MaterializeAggregationQueriesTransformer::needsMaterializedRelation(const AstAggregator& agg) {
    // everything with more than 1 body literal => materialize
    if (agg.getBodyLiterals().size() > 1) {
        return true;
    }

    // inspect remaining atom more closely
    const AstAtom* atom = dynamic_cast<const AstAtom*>(agg.getBodyLiterals()[0]);
    if (atom == nullptr) {
        // no atoms, so materialize
        return true;
    }

    // if the same variable occurs several times => materialize
    bool duplicates = false;
    std::set<std::string> vars;
    visitDepthFirst(*atom,
            [&](const AstVariable& var) { duplicates = duplicates || !vars.insert(var.getName()).second; });

    // if there are duplicates a materialization is required
    if (duplicates) {
        return true;
    }

    // for all others the materialization can be skipped
    return false;
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
                            res->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
                        }
                    } else {
                        res->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
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

                for (AstLiteral* lit : aggr->getBodyLiterals()) {
                    if (auto* bc = dynamic_cast<AstBooleanConstraint*>(lit)) {
                        bc->isTrue() ? containsTrue = true : containsFalse = true;
                    }
                }

                if (containsFalse || containsTrue) {
                    // Only keep literals that aren't boolean constraints
                    auto replacementAggregator = std::unique_ptr<AstAggregator>(aggr->clone());
                    replacementAggregator->clearBodyLiterals();

                    bool isEmpty = true;

                    // Don't bother copying over body literals if any are false
                    if (!containsFalse) {
                        for (AstLiteral* lit : aggr->getBodyLiterals()) {
                            // Don't add in 'true' boolean constraints
                            if (dynamic_cast<AstBooleanConstraint*>(lit) == nullptr) {
                                isEmpty = false;
                                replacementAggregator->addBodyLiteral(
                                        std::unique_ptr<AstLiteral>(lit->clone()));
                            }
                        }
                    }

                    if (containsFalse || isEmpty) {
                        // Empty aggregator body!
                        // Not currently handled, so add in a false literal in the body
                        // E.g. max x : { } =becomes=> max 1 : {0 = 1}
                        replacementAggregator->setTargetExpression(std::make_unique<AstNumericConstant>(1));

                        // Add '0 = 1' if false was found, '1 = 1' otherwise
                        int lhsConstant = containsFalse ? 0 : 1;
                        replacementAggregator->addBodyLiteral(std::make_unique<AstBinaryConstraint>(
                                BinaryConstraintOp::EQ, std::make_unique<AstNumericConstant>(lhsConstant),
                                std::make_unique<AstNumericConstant>(1)));
                    }

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
                        replacementClause->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
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
                    disconnectedClause->addToBody(std::unique_ptr<AstLiteral>(bodyLiteral->clone()));
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
        replacementClause->setHead(std::unique_ptr<AstAtom>(clause.getHead()->clone()));

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
                replacementClause->addToBody(std::unique_ptr<AstLiteral>(bodyLiteral->clone()));
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
                    newClause->setExecutionPlan(std::unique_ptr<AstExecutionPlan>(plan->clone()));
                }
                newClause->setHead(std::make_unique<AstAtom>(newRelationName.str()));
                for (AstLiteral* lit : clause->getBodyLiterals()) {
                    newClause->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
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

    struct nameVariables : public AstNodeMapper {
        mutable bool changed = false;
        mutable size_t count = 0;
        nameVariables() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (dynamic_cast<AstUnnamedVariable*>(node.get()) != nullptr) {
                changed = true;
                std::stringstream name;
                name << boundPrefix << "_" << count++;
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
                if (agg->getOperator() == AggregateOp::sum) {
                    if (const auto* constant =
                                    dynamic_cast<const AstNumericConstant*>(agg->getTargetExpression())) {
                        changed = true;
                        // Then construct the new thing to replace it with
                        auto count = std::make_unique<AstAggregator>(AggregateOp::count);
                        // Duplicate the body of the aggregate
                        for (const auto& lit : agg->getBodyLiterals()) {
                            count->addBodyLiteral(std::unique_ptr<AstLiteral>(lit->clone()));
                        }
                        auto number = std::unique_ptr<AstNumericConstant>(constant->clone());
                        // Now it's constant * count : { ... }
                        auto result = std::make_unique<AstIntrinsicFunctor>(
                                FunctorOp::MUL, std::move(number), std::move(count));

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
                constraints.insert(new AstBinaryConstraint(BinaryConstraintOp::EQ,
                        std::unique_ptr<AstArgument>(newVariable->clone()),
                        std::unique_ptr<AstArgument>(stringConstant->clone())));

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
                constraints.insert(
                        new AstBinaryConstraint(opEq, std::unique_ptr<AstArgument>(newVariable->clone()),
                                std::unique_ptr<AstArgument>(numberConstant->clone())));

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
                return std::unique_ptr<AstArgument>(cast->getValue()->clone());
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
                return isFloatType(typeAnalysis.getTypes(argument));
            };
            auto isUnsigned = [&](const AstArgument* argument) {
                return isUnsignedType(typeAnalysis.getTypes(argument));
            };
            auto isSymbol = [&](const AstArgument* argument) {
                return isSymbolType(typeAnalysis.getTypes(argument));
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

                        if (hasSignedType(types)) {
                            numericConstant->setType(AstNumericConstant::Type::Int);
                            changed = true;
                        } else if (hasUnsignedType(types)) {
                            numericConstant->setType(AstNumericConstant::Type::Uint);
                            changed = true;
                        } else if (hasFloatType(types)) {
                            numericConstant->setType(AstNumericConstant::Type::Float);
                            changed = true;
                        }
                    }
                }

                // Handle functor
                if (auto* functor = dynamic_cast<AstIntrinsicFunctor*>(node.get())) {
                    if (isOverloadedFunctor(functor->getFunction())) {
                        auto attemptOverload = [&](TypeAttribute ty, auto& fn) {
                            // All args must be of the same type.
                            if (!all_of(functor->getArguments(), fn)) return false;

                            functor->setFunction(convertOverloadedFunctor(functor->getFunction(), ty));
                            return true;
                        };

                        if (attemptOverload(TypeAttribute::Float, isFloat) ||
                                attemptOverload(TypeAttribute::Unsigned, isUnsigned) ||
                                attemptOverload(TypeAttribute::Symbol, isSymbol)) {
                            changed = true;
                        }
                    }
                }

                // Handle binary constraint
                if (auto* binaryConstraint = dynamic_cast<AstBinaryConstraint*>(node.get())) {
                    if (isOverloaded(binaryConstraint->getOperator())) {
                        // Get arguments
                        auto* leftArg = binaryConstraint->getLHS();
                        auto* rightArg = binaryConstraint->getRHS();

                        // Both args must be of the same type
                        if (isFloat(leftArg) && isFloat(rightArg)) {
                            BinaryConstraintOp convertedConstraint = convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Float);
                            binaryConstraint->setOperator(convertedConstraint);
                            changed = true;
                        } else if (isUnsigned(leftArg) && isUnsigned(rightArg)) {
                            BinaryConstraintOp convertedConstraint = convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Unsigned);
                            binaryConstraint->setOperator(convertedConstraint);
                            changed = true;
                        } else if (isSymbol(leftArg) && isSymbol(rightArg)) {
                            BinaryConstraintOp convertedConstraint = convertOverloadedConstraint(
                                    binaryConstraint->getOperator(), TypeAttribute::Symbol);
                            binaryConstraint->setOperator(convertedConstraint);
                            changed = true;
                        }
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

}  // end of namespace souffle
