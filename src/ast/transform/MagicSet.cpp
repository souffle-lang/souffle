/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MagicSet.cpp
 *
 * Define classes and functionality related to the magic set transformation.
 *
 ***********************************************************************/

#include "ast/transform/MagicSet.h"
#include "Global.h"
#include "ast/Aggregator.h"
#include "ast/Attribute.h"
#include "ast/BinaryConstraint.h"
#include "ast/Constant.h"
#include "ast/Directive.h"
#include "ast/Functor.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/StringConstant.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/analysis/IOType.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "ast/analysis/SCCGraph.h"
#include "ast/analysis/typesystem/PolymorphicObjects.h"
#include "ast/utility/BindingStore.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "parser/SrcLocation.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/RamTypes.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <optional>
#include <utility>

namespace souffle::ast::transform {
using NormaliseDatabaseTransformer = MagicSetTransformer::NormaliseDatabaseTransformer;
using LabelDatabaseTransformer = MagicSetTransformer::LabelDatabaseTransformer;
using AdornDatabaseTransformer = MagicSetTransformer::AdornDatabaseTransformer;
using MagicSetCoreTransformer = MagicSetTransformer::MagicSetCoreTransformer;

using NegativeLabellingTransformer =
        MagicSetTransformer::LabelDatabaseTransformer::NegativeLabellingTransformer;
using PositiveLabellingTransformer =
        MagicSetTransformer::LabelDatabaseTransformer::PositiveLabellingTransformer;

std::set<QualifiedName> MagicSetTransformer::getTriviallyIgnoredRelations(const TranslationUnit& tu) {
    const auto& program = tu.getProgram();
    const auto& ioTypes = tu.getAnalysis<analysis::IOTypeAnalysis>();
    std::set<QualifiedName> triviallyIgnoredRelations;

    // - Any relations known in constant time (IDB relations)
    for (auto* rel : program.getRelations()) {
        // Input relations
        if (ioTypes.isInput(rel)) {
            triviallyIgnoredRelations.insert(rel->getQualifiedName());
            continue;
        }

        // Any relations not dependent on any atoms
        bool hasRules = false;
        for (auto&& clause : program.getClauses(*rel)) {
            visit(clause->getBodyLiterals(), [&](const Atom& /* atom */) { hasRules = true; });
        }
        if (!hasRules) {
            triviallyIgnoredRelations.insert(rel->getQualifiedName());
        }
    }

    return triviallyIgnoredRelations;
}

std::set<QualifiedName> MagicSetTransformer::getWeaklyIgnoredRelations(const TranslationUnit& tu) {
    const auto& program = tu.getProgram();
    const auto& precedenceGraph = tu.getAnalysis<analysis::PrecedenceGraphAnalysis>().graph();
    const auto& polyAnalysis = tu.getAnalysis<analysis::PolymorphicObjectsAnalysis>();
    std::set<QualifiedName> weaklyIgnoredRelations;

    // Add magic-transform-exclude relations to the weakly ignored set
    for (const auto& relStr : splitString(Global::config().get("magic-transform-exclude"), ',')) {
        std::vector<std::string> qualifiers = splitString(relStr, '.');
        weaklyIgnoredRelations.insert(QualifiedName(qualifiers));
    }

    // Pick up specified relations from config
    std::set<QualifiedName> specifiedRelations;
    for (const auto& relStr : splitString(Global::config().get("magic-transform"), ',')) {
        std::vector<std::string> qualifiers = splitString(relStr, '.');
        specifiedRelations.insert(QualifiedName(qualifiers));
    }

    // Pick up specified relations and ignored relations from relation tags
    for (const auto* rel : program.getRelations()) {
        if (rel->hasQualifier(RelationQualifier::MAGIC)) {
            specifiedRelations.insert(rel->getQualifiedName());
        } else if (rel->hasQualifier(RelationQualifier::NO_MAGIC)) {
            weaklyIgnoredRelations.insert(rel->getQualifiedName());
        }
    }

    // Get the complement if not everything is magic'd
    std::set<QualifiedName> includedRelations = specifiedRelations - weaklyIgnoredRelations;
    if (!contains(includedRelations, "*")) {
        for (const Relation* rel : program.getRelations()) {
            if (!contains(specifiedRelations, rel->getQualifiedName())) {
                weaklyIgnoredRelations.insert(rel->getQualifiedName());
            }
        }
    }

    // - Add trivially computable relations
    for (const auto& relName : getTriviallyIgnoredRelations(tu)) {
        weaklyIgnoredRelations.insert(relName);
    }

    // - Any relation with a neglabel
    visit(program, [&](const Atom& atom) {
        const auto& qualifiers = atom.getQualifiedName().getQualifiers();
        if (!qualifiers.empty() && qualifiers[0] == "@neglabel") {
            weaklyIgnoredRelations.insert(atom.getQualifiedName());
        }
    });

    // - Any relation with a clause containing float-related binary constraints
    const std::set<BinaryConstraintOp> floatOps(
            {BinaryConstraintOp::FEQ, BinaryConstraintOp::FNE, BinaryConstraintOp::FLE,
                    BinaryConstraintOp::FGE, BinaryConstraintOp::FLT, BinaryConstraintOp::FGT});
    for (const auto* clause : program.getClauses()) {
        visit(*clause, [&](const BinaryConstraint& bc) {
            if (contains(floatOps, polyAnalysis.getOverloadedOperator(bc))) {
                weaklyIgnoredRelations.insert(clause->getHead()->getQualifiedName());
            }
        });
    }

    // - Any relation with a clause containing order-dependent functors
    const std::set<FunctorOp> orderDepFuncOps(
            {FunctorOp::MOD, FunctorOp::FDIV, FunctorOp::DIV, FunctorOp::UMOD});
    for (const auto* clause : program.getClauses()) {
        visit(*clause, [&](const IntrinsicFunctor& functor) {
            if (contains(orderDepFuncOps, polyAnalysis.getOverloadedFunctionOp(functor))) {
                weaklyIgnoredRelations.insert(clause->getHead()->getQualifiedName());
            }
        });
    }

    // - Any relation with functional dependencies
    for (auto* rel : program.getRelations()) {
        if (!rel->getFunctionalDependencies().empty()) {
            weaklyIgnoredRelations.insert(rel->getQualifiedName());
        }
    }

    // - Any relation with execution plans
    for (auto* clause : program.getClauses()) {
        if (clause->getExecutionPlan() != nullptr) {
            weaklyIgnoredRelations.insert(clause->getHead()->getQualifiedName());
        }
    }

    // - Any atom appearing in a clause containing a counter
    for (auto* clause : program.getClauses()) {
        bool containsCounter = false;
        visit(*clause, [&](const Counter& /* counter */) { containsCounter = true; });
        if (containsCounter) {
            visit(*clause, [&](const Atom& atom) { weaklyIgnoredRelations.insert(atom.getQualifiedName()); });
        }
    }

    // - Deal with strongly ignored relations
    const auto& stronglyIgnoredRelations = getStronglyIgnoredRelations(tu);

    // Add them in directly
    for (const auto& relName : stronglyIgnoredRelations) {
        weaklyIgnoredRelations.insert(relName);
    }

    // Add in any atoms whose magic rules might cause a need for neglabelling
    //  - Essentially, suppose R is strongly-ignored and A depends on R. Then, we must weakly ignore
    //    any relation that appears after A in any clause, otherwise a magic-set might be created that
    //    requires R to be neglabelled.
    for (const auto& relName : stronglyIgnoredRelations) {
        precedenceGraph.visit(program.getRelation(relName), [&](const auto* dependentRel) {
            const auto& depName = dependentRel->getQualifiedName();
            for (const auto* clause : program.getClauses()) {
                const auto& atoms = getBodyLiterals<Atom>(*clause);
                bool startIgnoring = false;
                for (const auto& atom : atoms) {
                    startIgnoring |= (atom->getQualifiedName() == depName);
                    if (startIgnoring) {
                        weaklyIgnoredRelations.insert(atom->getQualifiedName());
                    }
                }
            }
        });
    }

    return weaklyIgnoredRelations;
}

std::set<QualifiedName> MagicSetTransformer::getStronglyIgnoredRelations(const TranslationUnit& tu) {
    const auto& program = tu.getProgram();
    const auto& precedenceGraph = tu.getAnalysis<analysis::PrecedenceGraphAnalysis>().graph();
    std::set<QualifiedName> stronglyIgnoredRelations;

    // - Any atom appearing at the head of a clause containing a counter
    for (const auto* clause : program.getClauses()) {
        if (visitExists(*clause, [](const Counter&) { return true; })) {
            stronglyIgnoredRelations.insert(clause->getHead()->getQualifiedName());
        }
    }

    bool fixpointReached = false;
    while (!fixpointReached) {
        fixpointReached = true;
        // - To prevent poslabelling issues, all dependent strata should also be strongly ignored
        std::set<QualifiedName> dependentRelations;
        for (const auto& relName : stronglyIgnoredRelations) {
            precedenceGraph.visit(program.getRelation(relName), [&](const auto* dependentRel) {
                dependentRelations.insert(dependentRel->getQualifiedName());
            });
        }
        for (const auto& depRel : dependentRelations) {
            if (!contains(stronglyIgnoredRelations, depRel)) {
                fixpointReached = false;
                stronglyIgnoredRelations.insert(depRel);
            }
        }

        // - Since we can't duplicate the rules, nothing should be labelled in the bodies as well
        std::set<QualifiedName> bodyRelations;
        for (const auto& relName : stronglyIgnoredRelations) {
            visit(program.getClauses(relName),
                    [&](Atom& atom) { bodyRelations.insert(atom.getQualifiedName()); });
        }
        for (const auto& bodyRel : bodyRelations) {
            if (!contains(stronglyIgnoredRelations, bodyRel)) {
                fixpointReached = false;
                stronglyIgnoredRelations.insert(bodyRel);
            }
        }
    }

    return stronglyIgnoredRelations;
}

std::set<QualifiedName> MagicSetTransformer::getRelationsToNotLabel(const TranslationUnit& tu) {
    std::set<QualifiedName> result;
    for (const auto& name : getTriviallyIgnoredRelations(tu)) {
        result.insert(name);
    }
    for (const auto& name : getStronglyIgnoredRelations(tu)) {
        result.insert(name);
    }
    return result;
}

bool MagicSetTransformer::shouldRun(const TranslationUnit& tu) {
    const Program& program = tu.getProgram();
    if (Global::config().has("magic-transform")) return true;
    for (const auto* rel : program.getRelations()) {
        if (rel->hasQualifier(RelationQualifier::MAGIC)) return true;
    }
    return false;
}

bool NormaliseDatabaseTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;

    /** (1) Partition input and output relations */
    changed |= partitionIO(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();

    /** (2) Separate the IDB from the EDB */
    changed |= extractIDB(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();

    /** (3) Normalise arguments within each clause */
    changed |= normaliseArguments(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();

    /** (4) Querify output relations */
    changed |= querifyOutputRelations(translationUnit);
    if (changed) translationUnit.invalidateAnalyses();

    return changed;
}

bool NormaliseDatabaseTransformer::partitionIO(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    const auto& ioTypes = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();

    // Get all relations that are both input and output
    std::set<QualifiedName> relationsToSplit;
    for (auto* rel : program.getRelations()) {
        if (ioTypes.isInput(rel) && (ioTypes.isOutput(rel) || ioTypes.isPrintSize(rel))) {
            relationsToSplit.insert(rel->getQualifiedName());
        }
    }

    // For each of these relations I, add a new relation I' that's input instead.
    // The old relation I is no longer input, but copies over the data from I'.
    for (auto relName : relationsToSplit) {
        const auto* rel = program.getRelation(relName);
        assert(rel != nullptr && "relation does not exist");
        auto newRelName = QualifiedName(relName);
        newRelName.prepend("@split_in");

        // Create a new intermediate input relation, I'
        auto newRelation = mk<Relation>(newRelName);
        for (const auto* attr : rel->getAttributes()) {
            newRelation->addAttribute(clone(attr));
        }

        // Add the rule I <- I'
        auto newClause = mk<Clause>(relName);
        auto newHeadAtom = newClause->getHead();
        auto newBodyAtom = mk<Atom>(newRelName);
        for (std::size_t i = 0; i < rel->getArity(); i++) {
            std::stringstream varName;
            varName << "@var" << i;
            newHeadAtom->addArgument(mk<ast::Variable>(varName.str()));
            newBodyAtom->addArgument(mk<ast::Variable>(varName.str()));
        }
        newClause->addToBody(std::move(newBodyAtom));

        // New relation I' should be input, original should not
        VecOwn<Directive> iosToAdd;

        // New relation acquires the old relation's input directives
        // (leave any non-input directives as is)
        auto& info = program.getRelationInfo().at(relName);
        info.directives = filter(std::move(info.directives), [&](auto& io) {
            // only interested in acquiring the old relation's input directives
            if (io->getType() == ast::DirectiveType::input) {
                io->setQualifiedName(newRelName);
                iosToAdd.push_back(std::move(io));
                return false;
            }

            return true;
        });

        for (auto& io : iosToAdd) {
            program.addDirective(std::move(io));
        }

        // Add in the new relation and the copy clause
        program.addRelation(std::move(newRelation));
        program.addClause(std::move(newClause));
    }

    return !relationsToSplit.empty();
}

bool NormaliseDatabaseTransformer::extractIDB(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    const auto& ioTypes = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();

    // Helper method to check if an input relation has no associated rules
    auto isStrictlyEDB = [&](const Relation* rel) {
        bool hasRules = false;
        for (auto&& clause : program.getClauses(*rel)) {
            visit(clause->getBodyLiterals(), [&](const Atom& /* atom */) { hasRules = true; });
        }
        return !hasRules;
    };

    // Get all input relations that also have IDB rules attached
    std::set<QualifiedName> inputRelationNames;
    for (auto* rel : program.getRelations()) {
        if (ioTypes.isInput(rel) && !isStrictlyEDB(rel)) {
            assert(!ioTypes.isOutput(rel) && !ioTypes.isPrintSize(rel) &&
                    "input relations should not be output at this stage");
            inputRelationNames.insert(rel->getQualifiedName());
        }
    }

    // Add a new intermediate non-input relation for each
    // These will cover relation appearances in IDB rules
    std::map<QualifiedName, QualifiedName> inputToIntermediate;
    for (const auto& inputRelationName : inputRelationNames) {
        // Give it a unique name
        QualifiedName intermediateName(inputRelationName);
        intermediateName.prepend("@interm_in");
        inputToIntermediate[inputRelationName] = intermediateName;

        // Add the relation
        auto intermediateRelation = clone(program.getRelation(inputRelationName));
        intermediateRelation->setQualifiedName(intermediateName);
        program.addRelation(std::move(intermediateRelation));
    }

    // Rename them systematically
    renameAtoms(program, inputToIntermediate);

    // Add the rule I' <- I
    for (const auto& inputRelationName : inputRelationNames) {
        auto queryHead = mk<Atom>(inputToIntermediate.at(inputRelationName));
        auto queryLiteral = mk<Atom>(inputRelationName);

        // Give them identical arguments
        const auto* inputRelation = program.getRelation(inputRelationName);
        for (std::size_t i = 0; i < inputRelation->getArity(); i++) {
            std::stringstream var;
            var << "@query_x" << i;
            queryHead->addArgument(mk<ast::Variable>(var.str()));
            queryLiteral->addArgument(mk<ast::Variable>(var.str()));
        }

        auto query = mk<Clause>(std::move(queryHead));
        query->addToBody(std::move(queryLiteral));
        program.addClause(std::move(query));
    }

    return !inputRelationNames.empty();
}

bool NormaliseDatabaseTransformer::querifyOutputRelations(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    // Helper method to check if a relation is a single-rule output query
    auto isStrictlyOutput = [&](const Relation* rel) {
        bool strictlyOutput = true;
        std::size_t ruleCount = 0;

        for (const auto* clause : program.getClauses()) {
            // Check if the relation is used in the body of any rules
            visit(clause->getBodyLiterals(), [&](const Atom& atom) {
                if (atom.getQualifiedName() == rel->getQualifiedName()) {
                    strictlyOutput = false;
                }
            });

            // Keep track of number of rules defining the relation
            if (clause->getHead()->getQualifiedName() == rel->getQualifiedName()) {
                ruleCount++;
            }
        }

        return strictlyOutput && ruleCount <= 1;
    };

    // Get all output relations that need to be normalised
    const auto& ioTypes = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();
    std::set<QualifiedName> outputRelationNames;
    for (auto* rel : program.getRelations()) {
        if ((ioTypes.isOutput(rel) || ioTypes.isPrintSize(rel)) && !isStrictlyOutput(rel)) {
            assert(!ioTypes.isInput(rel) && "output relations should not be input at this stage");
            outputRelationNames.insert(rel->getQualifiedName());
        }
    }

    // Add a new intermediate non-output relation for each
    // These will cover relation appearances in intermediate rules
    std::map<QualifiedName, QualifiedName> outputToIntermediate;
    for (const auto& outputRelationName : outputRelationNames) {
        // Give it a unique name
        QualifiedName intermediateName(outputRelationName);
        intermediateName.prepend("@interm_out");
        outputToIntermediate[outputRelationName] = intermediateName;

        // Add the relation
        auto intermediateRelation = clone(program.getRelation(outputRelationName));
        intermediateRelation->setQualifiedName(intermediateName);
        program.addRelation(std::move(intermediateRelation));
    }

    // Rename them systematically
    renameAtoms(program, outputToIntermediate);

    // Add the rule I <- I'
    for (const auto& outputRelationName : outputRelationNames) {
        auto queryHead = mk<Atom>(outputRelationName);
        auto queryLiteral = mk<Atom>(outputToIntermediate.at(outputRelationName));

        // Give them identical arguments
        const auto* outputRelation = program.getRelation(outputRelationName);
        for (std::size_t i = 0; i < outputRelation->getArity(); i++) {
            std::stringstream var;
            var << "@query_x" << i;
            queryHead->addArgument(mk<ast::Variable>(var.str()));
            queryLiteral->addArgument(mk<ast::Variable>(var.str()));
        }
        auto query = mk<Clause>(std::move(queryHead));
        query->addToBody(std::move(queryLiteral));
        program.addClause(std::move(query));
    }

    return !outputRelationNames.empty();
}

bool NormaliseDatabaseTransformer::normaliseArguments(TranslationUnit& translationUnit) {
    using Constaints = VecOwn<BinaryConstraint>;

    Program& program = translationUnit.getProgram();

    // Replace all non-variable-arguments nested inside the node with named variables
    // Also, keeps track of constraints to add to keep the clause semantically equivalent
    struct argument_normaliser : public NodeMapper {
        Constaints& constraints;
        int& changeCount;

        argument_normaliser(Constaints& constraints, int& changeCount)
                : constraints(constraints), changeCount(changeCount) {}

        Own<Node> operator()(Own<Node> node) const override {
            if (auto* aggr = as<Aggregator>(node)) {
                // Aggregator variable scopes should be maintained, so changes shouldn't propagate
                // above this level.
                Constaints subConstraints;
                argument_normaliser aggrUpdate(subConstraints, changeCount);
                aggr->apply(aggrUpdate);

                // Add the constraints to this level
                VecOwn<Literal> newBodyLiterals;
                append(newBodyLiterals, cloneRange(aggr->getBodyLiterals()));
                append(newBodyLiterals, cloneRange(subConstraints));

                // Update the node to reflect normalised aggregator
                node = aggr->getTargetExpression() != nullptr
                               ? mk<Aggregator>(aggr->getBaseOperator(), clone(aggr->getTargetExpression()),
                                         std::move(newBodyLiterals))
                               : mk<Aggregator>(aggr->getBaseOperator(), nullptr, std::move(newBodyLiterals));
            } else {
                // Otherwise, just normalise children as usual.
                node->apply(*this);
            }

            // All non-variables should be normalised
            if (auto* arg = as<Argument>(node)) {
                if (!isA<ast::Variable>(arg)) {
                    std::stringstream name;
                    name << "@abdul" << changeCount++;

                    // Unnamed variables don't need a new constraint, just give them a name
                    if (isA<UnnamedVariable>(arg)) {
                        return mk<ast::Variable>(name.str());
                    }

                    // Link other variables back to their original value with a `<var> = <arg>` constraint
                    constraints.push_back(mk<BinaryConstraint>(
                            BinaryConstraintOp::EQ, mk<ast::Variable>(name.str()), clone(arg)));
                    return mk<ast::Variable>(name.str());
                }
            }
            return node;
        }
    };

    // Transform each clause so that all arguments are:
    //      1) a variable, or
    //      2) the RHS of a `<var> = <arg>` constraint
    bool changed = false;
    for (auto* clause : program.getClauses()) {
        int changeCount = 0;
        Constaints constraintsToAdd;
        argument_normaliser update(constraintsToAdd, changeCount);

        // Apply to each clause head
        clause->getHead()->apply(update);

        // Apply to each body literal that isn't already a `<var> = <arg>` constraint
        for (Literal* lit : clause->getBodyLiterals()) {
            if (auto* bc = as<BinaryConstraint>(lit)) {
                if (isEqConstraint(bc->getBaseOperator()) && isA<ast::Variable>(bc->getLHS())) {
                    continue;
                }
            }
            lit->apply(update);
        }

        // Also apply to each record
        visit(*clause, [&](const RecordInit& rec) {
            for (Argument* arg : rec.getArguments()) {
                arg->apply(update);
            }
        });

        clause->addToBody(clone<Literal>(constraintsToAdd));
        changed |= changeCount != 0;
    }

    return changed;
}

QualifiedName AdornDatabaseTransformer::getAdornmentID(
        const QualifiedName& relName, const std::string& adornmentMarker) {
    if (adornmentMarker == "") return relName;
    QualifiedName adornmentID(relName);
    std::stringstream adornmentMarkerRepr;
    adornmentMarkerRepr << "{" << adornmentMarker << "}";
    adornmentID.append(adornmentMarkerRepr.str());
    return adornmentID;
}

Own<Clause> AdornDatabaseTransformer::adornClause(const Clause* clause, const std::string& adornmentMarker) {
    const auto& relName = clause->getHead()->getQualifiedName();
    const auto& headArgs = clause->getHead()->getArguments();
    BindingStore variableBindings(clause);

    /* Note that variables can be bound through:
     *  (1) an appearance in a body atom (strong)
     *  (2) an appearance in a bound field of the head atom (weak)
     *  (3) equality with a fully bound functor (via dependency analysis)
     *
     * When computing (3), appearances (1) and (2) must be separated to maintain the termination semantics of
     * the original program. Functor variables are not considered bound if they are only bound via the head.
     *
     * Justification: Suppose a new variable Y is marked as bound because of its appearance in a functor
     * Y=X+1, and X was already found to be bound:
     *  (1) If X was bound through a body atom, then the behaviour of typical magic-set is exhibited, where
     * the magic-set of Y is bounded by the values that X can take, which is bounded by induction.
     *  (2) If X was bound only through the head atom, then Y is only fixed to an appearance in a magic-atom.
     * In the presence of recursion, this can potentially lead to an infinitely-sized magic-set for an atom.
     *
     * Therefore, bound head atom vars should be marked as weakly bound.
     */
    for (std::size_t i = 0; i < adornmentMarker.length(); i++) {
        const auto* var = as<ast::Variable>(headArgs[i]);
        assert(var != nullptr && "expected only variables in head");
        if (adornmentMarker[i] == 'b') {
            variableBindings.bindVariableWeakly(var->getName());
        }
    }

    // Create the adorned clause with an empty body
    auto adornedClause = mk<Clause>(getAdornmentID(relName, adornmentMarker));

    // Copy over plans if needed
    if (clause->getExecutionPlan() != nullptr) {
        assert(contains(weaklyIgnoredRelations, clause->getHead()->getQualifiedName()) &&
                "clauses with plans should be ignored");
        adornedClause->setExecutionPlan(clone(clause->getExecutionPlan()));
    }

    // Create the head atom
    auto adornedHeadAtom = adornedClause->getHead();
    assert((adornmentMarker == "" || headArgs.size() == adornmentMarker.length()) &&
            "adornment marker should correspond to head atom variables");
    for (const auto* arg : headArgs) {
        const auto* var = as<ast::Variable>(arg);
        assert(var != nullptr && "expected only variables in head");
        adornedHeadAtom->addArgument(clone(var));
    }

    // Add in adorned body literals
    VecOwn<Literal> adornedBodyLiterals;
    for (const auto* lit : clause->getBodyLiterals()) {
        if (const auto* negation = as<Negation>(lit)) {
            // Negated atoms should not be adorned, but their clauses should be anyway
            const auto negatedAtomName = negation->getAtom()->getQualifiedName();
            assert(contains(weaklyIgnoredRelations, negatedAtomName) &&
                    "negated atoms should not be adorned");
            queueAdornment(negatedAtomName, "");
        }

        if (!isA<Atom>(lit)) {
            // Non-atoms are added directly
            adornedBodyLiterals.push_back(clone(lit));
            continue;
        }

        const auto* atom = as<Atom>(lit);
        assert(atom != nullptr && "expected atom");

        // Form the appropriate adornment marker
        std::stringstream atomAdornment;
        if (!contains(weaklyIgnoredRelations, atom->getQualifiedName())) {
            for (const auto* arg : atom->getArguments()) {
                const auto* var = as<ast::Variable>(arg);
                assert(var != nullptr && "expected only variables in atom");
                atomAdornment << (variableBindings.isBound(var->getName()) ? "b" : "f");
            }
        }
        auto currAtomAdornmentID = getAdornmentID(atom->getQualifiedName(), atomAdornment.str());

        // Add to the ToDo queue if needed
        queueAdornment(atom->getQualifiedName(), atomAdornment.str());

        // Add the adorned version to the clause
        auto adornedBodyAtom = clone(atom);
        adornedBodyAtom->setQualifiedName(currAtomAdornmentID);
        adornedBodyLiterals.push_back(std::move(adornedBodyAtom));

        // All arguments are now bound
        for (const auto* arg : atom->getArguments()) {
            const auto* var = as<ast::Variable>(arg);
            assert(var != nullptr && "expected only variables in atom");
            variableBindings.bindVariableStrongly(var->getName());
        }
    }
    adornedClause->setBodyLiterals(std::move(adornedBodyLiterals));

    return adornedClause;
}

bool AdornDatabaseTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    const auto& ioTypes = translationUnit.getAnalysis<analysis::IOTypeAnalysis>();
    weaklyIgnoredRelations = getWeaklyIgnoredRelations(translationUnit);

    // Output relations trigger the adornment process
    for (const auto* rel : program.getRelations()) {
        if (ioTypes.isOutput(rel) || ioTypes.isPrintSize(rel)) {
            queueAdornment(rel->getQualifiedName(), "");
        }
    }

    bool changed = false;

    // Keep going while there's things to adorn
    while (hasAdornmentToProcess()) {
        // Pop off the next head adornment to do
        const auto& [relName, adornmentMarker] = nextAdornmentToProcess();

        // Add the adorned relation if needed
        if (adornmentMarker != "") {
            const auto* rel = program.getRelation(relName);
            assert(rel != nullptr && "relation does not exist");

            // TODO: How does this interact with fundeps? Should they be copied over as well?
            auto adornedRelation = mk<Relation>(getAdornmentID(relName, adornmentMarker));
            for (const auto* attr : rel->getAttributes()) {
                adornedRelation->addAttribute(clone(attr));
            }
            program.addRelation(std::move(adornedRelation));
        }

        // Adorn every clause correspondingly
        for (auto&& clause : program.getClauses(relName)) {
            changed = true;
            program.addClause(adornClause(&*clause, adornmentMarker));

            if (adornmentMarker == "") {
                program.removeClause(*clause);
            }
        }
    }

    return changed;
}

QualifiedName NegativeLabellingTransformer::getNegativeLabel(const QualifiedName& name) {
    QualifiedName newName(name);
    newName.prepend("@neglabel");
    return newName;
}

QualifiedName PositiveLabellingTransformer::getPositiveLabel(const QualifiedName& name, std::size_t count) {
    std::stringstream label;
    label << "@poscopy_" << count;
    QualifiedName labelledName(name);
    labelledName.prepend(label.str());
    return labelledName;
}

bool LabelDatabaseTransformer::isNegativelyLabelled(const QualifiedName& name) {
    auto qualifiers = name.getQualifiers();
    assert(!qualifiers.empty() && "unexpected empty qualifier list");
    return qualifiers[0] == "@neglabel";
}

bool NegativeLabellingTransformer::transform(TranslationUnit& translationUnit) {
    const auto& sccGraph = translationUnit.getAnalysis<analysis::SCCGraphAnalysis>();
    Program& program = translationUnit.getProgram();

    std::set<QualifiedName> relationsToLabel;
    VecOwn<Clause> clausesToAdd;
    const auto& relationsToNotLabel = getRelationsToNotLabel(translationUnit);

    // Negatively label all relations that might affect stratification after MST
    //      - Negated relations
    //      - Relations that appear in aggregators
    visit(program, [&](const Negation& neg) {
        auto* atom = neg.getAtom();
        auto relName = atom->getQualifiedName();
        if (contains(relationsToNotLabel, relName)) return;
        atom->setQualifiedName(getNegativeLabel(relName));
        relationsToLabel.insert(relName);
    });
    visit(program, [&](Aggregator& aggr) {
        visit(aggr, [&](Atom& atom) {
            auto relName = atom.getQualifiedName();
            if (contains(relationsToNotLabel, relName)) return;
            atom.setQualifiedName(getNegativeLabel(relName));
            relationsToLabel.insert(relName);
        });
    });

    // Copy over the rules for labelled relations one stratum at a time
    for (std::size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        // Check which relations to label in this stratum
        const auto& stratumRels = sccGraph.getInternalRelations(stratum);
        std::map<QualifiedName, QualifiedName> newSccFriendNames;
        for (const auto* rel : stratumRels) {
            auto relName = rel->getQualifiedName();
            if (contains(relationsToNotLabel, relName)) continue;
            relationsToLabel.insert(relName);
            newSccFriendNames[relName] = getNegativeLabel(relName);
        }

        // Negatively label the relations in a new copy of this stratum
        for (const auto* rel : stratumRels) {
            if (contains(relationsToNotLabel, rel->getQualifiedName())) continue;

            for (auto&& clause : program.getClauses(*rel)) {
                auto neggedClause = clone(clause);
                renameAtoms(neggedClause, newSccFriendNames);
                clausesToAdd.push_back(std::move(neggedClause));
            }
        }
    }

    // Add in all the relations that were labelled
    for (const auto& relName : relationsToLabel) {
        const auto* originalRel = program.getRelation(relName);
        assert(originalRel != nullptr && "unlabelled relation does not exist");
        auto labelledRelation = clone(originalRel);
        labelledRelation->setQualifiedName(getNegativeLabel(relName));
        program.addRelation(std::move(labelledRelation));
    }

    // Add in all the negged clauses
    for (auto&& clause : clausesToAdd) {
        program.addClause(std::move(clause));
    }

    return !relationsToLabel.empty();
}

bool PositiveLabellingTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    const auto& sccGraph = translationUnit.getAnalysis<analysis::SCCGraphAnalysis>();
    const auto& precedenceGraph = translationUnit.getAnalysis<analysis::PrecedenceGraphAnalysis>().graph();
    const auto& relationsToNotLabel = getRelationsToNotLabel(translationUnit);

    // Partition the strata into neglabelled and regular
    std::set<std::size_t> neglabelledStrata;
    std::map<std::size_t, std::size_t> originalStrataCopyCount;
    for (std::size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        std::size_t numNeggedRelations = 0;
        const auto& stratumRels = sccGraph.getInternalRelations(stratum);

        // Count how many relations in this node are neglabelled
        for (const auto* rel : stratumRels) {
            if (isNegativelyLabelled(rel->getQualifiedName())) {
                numNeggedRelations++;
            }
        }
        assert((numNeggedRelations == 0 || numNeggedRelations == stratumRels.size()) &&
                "stratum cannot contain a mix of neglabelled and unlabelled relations");

        if (numNeggedRelations > 0) {
            // This is a neglabelled stratum that will not be copied
            neglabelledStrata.insert(stratum);
        } else {
            // This is a regular stratum that may be copied
            originalStrataCopyCount[stratum] = 0;
        }
    }

    // Keep track of strata that depend on each stratum
    // e.g. T in dependentStrata[S] iff a relation in T depends on a relation in S
    std::map<std::size_t, std::set<std::size_t>> dependentStrata;
    for (std::size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        dependentStrata[stratum] = std::set<std::size_t>();
    }
    for (const auto* rel : program.getRelations()) {
        std::size_t stratum = sccGraph.getSCC(rel);
        precedenceGraph.visit(rel, [&](const auto* dependentRel) {
            dependentStrata[stratum].insert(sccGraph.getSCC(dependentRel));
        });
    }

    // Label the positive derived literals in the clauses of neglabelled relations
    // Need a new copy of those relations up to that point for each
    for (std::size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        if (!contains(neglabelledStrata, stratum)) continue;

        // Rename in the current stratum
        for (const auto* rel : sccGraph.getInternalRelations(stratum)) {
            assert(isNegativelyLabelled(rel->getQualifiedName()) &&
                    "should only be looking at neglabelled strata");
            std::set<QualifiedName> relsToCopy;

            // Get the unignored unlabelled relations appearing in the rules
            for (auto&& clause : program.getClauses(*rel)) {
                visit(*clause, [&](const Atom& atom) {
                    const auto& name = atom.getQualifiedName();
                    if (!contains(relationsToNotLabel, name) && !isNegativelyLabelled(name)) {
                        relsToCopy.insert(name);
                    }
                });
            }

            std::map<QualifiedName, QualifiedName> labelledNames;
            for (const auto& relName : relsToCopy) {
                std::size_t relStratum = sccGraph.getSCC(program.getRelation(relName));
                std::size_t copyCount = originalStrataCopyCount.at(relStratum) + 1;
                labelledNames[relName] = getPositiveLabel(relName, copyCount);
            }

            // Positively label them
            renameAtoms(program, rel->getQualifiedName(), labelledNames);
        }

        // Create the rules (from all previous strata) for the newly positive labelled literals
        for (std::size_t pos = stratum; pos > 0; pos--) {
            const std::size_t preStratum = pos - 1;
            if (contains(neglabelledStrata, preStratum)) continue;
            if (!contains(dependentStrata[preStratum], stratum)) continue;

            for (const auto* rel : sccGraph.getInternalRelations(preStratum)) {
                if (contains(relationsToNotLabel, rel->getQualifiedName())) continue;

                for (auto&& clause : program.getClauses(*rel)) {
                    // Grab the new names for all unignored unlabelled positive atoms
                    std::map<QualifiedName, QualifiedName> labelledNames;
                    visit(*clause, [&](const Atom& atom) {
                        const auto& relName = atom.getQualifiedName();
                        if (contains(relationsToNotLabel, relName) || isNegativelyLabelled(relName)) return;
                        std::size_t relStratum = sccGraph.getSCC(program.getRelation(relName));
                        std::size_t copyCount = originalStrataCopyCount.at(relStratum) + 1;
                        labelledNames[relName] = getPositiveLabel(relName, copyCount);
                    });

                    // Rename atoms accordingly
                    auto labelledClause = clone(clause);
                    renameAtoms(labelledClause, labelledNames);
                    program.addClause(std::move(labelledClause));
                }
            }

            originalStrataCopyCount[preStratum]++;
        }
    }

    // Add each copy for each relation in
    bool changed = false;
    for (const auto& [stratum, numCopies] : originalStrataCopyCount) {
        const auto& stratumRels = sccGraph.getInternalRelations(stratum);
        for (std::size_t copy = 0; copy < numCopies; copy++) {
            for (auto* rel : stratumRels) {
                auto newRelation = clone(rel);
                newRelation->setQualifiedName(getPositiveLabel(newRelation->getQualifiedName(), copy + 1));
                program.addRelation(std::move(newRelation));
                changed = true;
            }
        }
    }
    return changed;
}

bool MagicSetCoreTransformer::isAdorned(const QualifiedName& name) {
    // Grab the final qualifier - this is where the adornment is if it exists
    auto qualifiers = name.getQualifiers();
    assert(!qualifiers.empty() && "unexpected empty qualifier list");
    auto finalQualifier = qualifiers[qualifiers.size() - 1];
    assert(finalQualifier.length() > 0 && "unexpected empty qualifier");

    // Pattern: {[bf]*}
    if (finalQualifier[0] == '{' && finalQualifier[finalQualifier.length() - 1] == '}') {
        for (std::size_t i = 1; i < finalQualifier.length() - 1; i++) {
            char curBindingType = finalQualifier[i];
            if (curBindingType != 'b' && curBindingType != 'f') {
                return false;
            }
        }
        return true;
    }
    return false;
}

std::string MagicSetCoreTransformer::getAdornment(const QualifiedName& name) {
    assert(isAdorned(name) && "relation not adorned");
    auto qualifiers = name.getQualifiers();
    auto finalQualifier = qualifiers[qualifiers.size() - 1];
    std::stringstream binding;
    for (std::size_t i = 1; i < finalQualifier.length() - 1; i++) {
        binding << finalQualifier[i];
    }
    return binding.str();
}

QualifiedName MagicSetCoreTransformer::getMagicName(const QualifiedName& name) {
    assert(isAdorned(name) && "cannot magify unadorned predicates");
    QualifiedName magicRelName(name);
    magicRelName.prepend("@magic");
    return magicRelName;
}

Own<Atom> MagicSetCoreTransformer::createMagicAtom(const Atom* atom) {
    auto origRelName = atom->getQualifiedName();
    auto args = atom->getArguments();

    auto magicAtom = mk<Atom>(getMagicName(origRelName));

    auto adornmentMarker = getAdornment(origRelName);
    for (std::size_t i = 0; i < args.size(); i++) {
        if (adornmentMarker[i] == 'b') {
            magicAtom->addArgument(clone(args[i]));
        }
    }

    return magicAtom;
}

void MagicSetCoreTransformer::addRelevantVariables(
        std::set<std::string>& variables, const std::vector<const BinaryConstraint*> eqConstraints) {
    // Helper method to check if all variables in an argument are bound
    auto isFullyBound = [&](const Argument* arg) {
        bool fullyBound = true;
        visit(*arg, [&](const ast::Variable& var) { fullyBound &= contains(variables, var.getName()); });
        return fullyBound;
    };

    // Helper method to add all newly relevant variables given a lhs = rhs constraint
    auto addLocallyRelevantVariables = [&](const Argument* lhs, const Argument* rhs) {
        const auto* lhsVar = as<ast::Variable>(lhs);
        if (lhsVar == nullptr) return true;

        // if the rhs is fully bound, lhs is now bound
        if (!contains(variables, lhsVar->getName())) {
            if (isFullyBound(rhs)) {
                variables.insert(lhsVar->getName());
                return false;
            } else {
                return true;
            }
        }

        // if the rhs is a record, and lhs is a bound var, then all rhs vars are bound
        bool fixpointReached = true;
        if (const auto* rhsRec = as<RecordInit>(rhs)) {
            for (const auto* arg : rhsRec->getArguments()) {
                const auto* subVar = as<ast::Variable>(arg);
                assert(subVar != nullptr && "expected only variable arguments");
                if (!contains(variables, subVar->getName())) {
                    fixpointReached = false;
                    variables.insert(subVar->getName());
                }
            }
        }

        return fixpointReached;
    };

    // Keep adding in relevant variables until we reach a fixpoint
    bool fixpointReached = false;
    while (!fixpointReached) {
        fixpointReached = true;
        for (const auto* eqConstraint : eqConstraints) {
            assert(isEqConstraint(eqConstraint->getBaseOperator()) && "expected only eq constraints");
            fixpointReached &= addLocallyRelevantVariables(eqConstraint->getLHS(), eqConstraint->getRHS());
            fixpointReached &= addLocallyRelevantVariables(eqConstraint->getRHS(), eqConstraint->getLHS());
        }
    }
}

Own<Clause> MagicSetCoreTransformer::createMagicClause(const Atom* atom,
        const VecOwn<Atom>& constrainingAtoms, const std::vector<const BinaryConstraint*> eqConstraints) {
    auto magicClause = mk<Clause>(createMagicAtom(atom));
    // Add in all constraining atoms
    magicClause->setBodyLiterals(clone<Literal>(constrainingAtoms));
    auto magicHead = magicClause->getHead();

    // Get the set of all variables that will be relevant to the magic clause
    std::set<std::string> relevantVariables;
    visit(constrainingAtoms, [&](const ast::Variable& var) { relevantVariables.insert(var.getName()); });
    visit(*magicHead, [&](const ast::Variable& var) { relevantVariables.insert(var.getName()); });
    addRelevantVariables(relevantVariables, eqConstraints);

    // Add in all eq constraints containing ONLY relevant variables
    for (const auto* eqConstraint : eqConstraints) {
        bool addConstraint = true;
        visit(*eqConstraint, [&](const ast::Variable& var) {
            if (!contains(relevantVariables, var.getName())) {
                addConstraint = false;
            }
        });

        if (addConstraint) magicClause->addToBody(clone(eqConstraint));
    }

    return magicClause;
}

std::vector<const BinaryConstraint*> MagicSetCoreTransformer::getBindingEqualityConstraints(
        const Clause* clause) {
    std::vector<const BinaryConstraint*> equalityConstraints;
    for (const auto* lit : clause->getBodyLiterals()) {
        const auto* bc = as<BinaryConstraint>(lit);
        if (bc == nullptr || !isEqConstraint(bc->getBaseOperator())) continue;
        if (isA<ast::Variable>(bc->getLHS()) || isA<Constant>(bc->getRHS())) {
            bool containsAggrs = false;
            visit(*bc, [&](const Aggregator& /* aggr */) { containsAggrs = true; });
            if (!containsAggrs) {
                equalityConstraints.push_back(bc);
            }
        }
    }
    return equalityConstraints;
}

bool MagicSetCoreTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();

    /** Perform the Magic Set Transformation */
    for (auto* clause : program.getClauses()) {
        auto adorned = isAdorned(ast::getName(*clause));

        // (1) Add the associated magic rules
        auto eqConstraints = getBindingEqualityConstraints(clause);
        VecOwn<Atom> atomsToTheLeft;
        if (adorned) {
            // Add the specialising head atom
            // Output relations are not specialised, and so the head will not contribute to specialisation
            atomsToTheLeft.push_back(createMagicAtom(clause->getHead()));
        }

        for (const auto* lit : clause->getBodyLiterals()) {
            if (auto* atom = as<Atom>(lit)) {
                if (isAdorned(atom->getQualifiedName())) {
                    program.addClause(createMagicClause(atom, atomsToTheLeft, eqConstraints));
                }

                atomsToTheLeft.push_back(clone(atom));
            }
        }

        // (2) Refined clause by adding magic atom.
        //     Unadorned relations need not be refined, as every possible tuple is relevant.
        if (adorned) {
            // Refine the clause with a prepended magic atom
            auto lits = clone(clause->getBodyLiterals());
            lits.insert(lits.begin(), createMagicAtom(clause->getHead()));
            // modify in place instead of clone/add/remove-old in order to keep AST diffs pretty
            clause->setBodyLiterals(std::move(lits));
        }
    }

    // Add in the magic relations
    bool changed = false;
    for (const auto* rel : program.getRelations()) {
        const auto& origName = rel->getQualifiedName();
        if (!isAdorned(origName)) continue;
        auto magicRelation = mk<Relation>(getMagicName(origName));
        auto attributes = program.getRelation(origName)->getAttributes();
        auto adornmentMarker = getAdornment(origName);
        for (std::size_t i = 0; i < attributes.size(); i++) {
            if (adornmentMarker[i] == 'b') {
                magicRelation->addAttribute(clone(attributes[i]));
            }
        }
        changed = true;
        program.addRelation(std::move(magicRelation));
    }
    return changed;
}

}  // namespace souffle::ast::transform
