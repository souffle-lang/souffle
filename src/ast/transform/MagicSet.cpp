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
#include "RamTypes.h"
#include "ast/Aggregator.h"
#include "ast/Attribute.h"
#include "ast/BinaryConstraint.h"
#include "ast/Constant.h"
#include "ast/Functor.h"
#include "ast/IO.h"
#include "ast/Node.h"
#include "ast/NodeMapper.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/StringConstant.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Utils.h"
#include "ast/analysis/IOType.h"
#include "ast/analysis/PrecedenceGraph.h"
#include "ast/analysis/SCCGraph.h"
#include "parser/BinaryConstraintOps.h"
#include "parser/SrcLocation.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <optional>
#include <utility>

namespace souffle {
typedef MagicSetTransformer::NormaliseDatabaseTransformer NormaliseDatabaseTransformer;
typedef MagicSetTransformer::LabelDatabaseTransformer LabelDatabaseTransformer;
typedef MagicSetTransformer::AdornDatabaseTransformer AdornDatabaseTransformer;
typedef MagicSetTransformer::MagicSetCoreTransformer MagicSetCoreTransformer;
typedef MagicSetTransformer::AdornDatabaseTransformer::BindingStore BindingStore;

typedef MagicSetTransformer::LabelDatabaseTransformer::NegativeLabellingTransformer
        NegativeLabellingTransformer;
typedef MagicSetTransformer::LabelDatabaseTransformer::PositiveLabellingTransformer
        PositiveLabellingTransformer;

std::set<AstQualifiedName> MagicSetTransformer::getIgnoredRelations(const AstTranslationUnit& tu) {
    const auto& program = *tu.getProgram();
    const auto& ioTypes = *tu.getAnalysis<IOType>();

    std::set<AstQualifiedName> relationsToIgnore;

    // - Any relations not specified to magic-set
    std::vector<AstQualifiedName> specifiedRelations;

    // Pick up specified relations from config
    std::vector<std::string> configRels = splitString(Global::config().get("magic-transform"), ',');
    for (const auto& relStr : configRels) {
        std::vector<std::string> qualifiers = splitString(relStr, '.');
        specifiedRelations.push_back(AstQualifiedName(qualifiers));
    }

    // Pick up specified relations from relation tags
    for (const auto* rel : program.getRelations()) {
        if (rel->hasQualifier(RelationQualifier::MAGIC)) {
            specifiedRelations.push_back(rel->getQualifiedName());
        }
    }

    // Get the complement if not everything is magic'd
    if (!contains(configRels, "*")) {
        for (const AstRelation* rel : program.getRelations()) {
            if (!contains(specifiedRelations, rel->getQualifiedName())) {
                relationsToIgnore.insert(rel->getQualifiedName());
            }
        }
    }

    // - Any relations known in constant time (IDB relations)
    for (auto* rel : program.getRelations()) {
        // Input relations
        if (ioTypes.isInput(rel)) {
            relationsToIgnore.insert(rel->getQualifiedName());
            continue;
        }

        // Any relations not dependent on any atoms
        bool hasRules = false;
        for (const auto* clause : getClauses(program, rel->getQualifiedName())) {
            visitDepthFirst(clause->getBodyLiterals(), [&](const AstAtom& /* atom */) { hasRules = true; });
        }
        if (!hasRules) {
            relationsToIgnore.insert(rel->getQualifiedName());
        }
    }

    // - Any relation with a neglabel
    visitDepthFirst(program, [&](const AstAtom& atom) {
        const auto& qualifiers = atom.getQualifiedName().getQualifiers();
        if (!qualifiers.empty() && qualifiers[0] == "@neglabel") {
            relationsToIgnore.insert(atom.getQualifiedName());
        }
    });

    // - Any relation with a clause containing float-related binary constraints
    const std::set<BinaryConstraintOp> floatOps(
            {BinaryConstraintOp::FEQ, BinaryConstraintOp::FNE, BinaryConstraintOp::FLE,
                    BinaryConstraintOp::FGE, BinaryConstraintOp::FLT, BinaryConstraintOp::FGT});
    for (const auto* clause : program.getClauses()) {
        visitDepthFirst(*clause, [&](const AstBinaryConstraint& bc) {
            if (contains(floatOps, bc.getOperator())) {
                relationsToIgnore.insert(clause->getHead()->getQualifiedName());
            }
        });
    }

    // - Any relation with a clause containing order-dependent functors
    const std::set<FunctorOp> orderDepFuncOps(
            {FunctorOp::MOD, FunctorOp::FDIV, FunctorOp::DIV, FunctorOp::UMOD});
    for (const auto* clause : program.getClauses()) {
        visitDepthFirst(*clause, [&](const AstIntrinsicFunctor& functor) {
            if (contains(orderDepFuncOps, functor.getFunctionInfo()->op)) {
                relationsToIgnore.insert(clause->getHead()->getQualifiedName());
            }
        });
    }

    // - Any eqrel relation
    for (auto* rel : program.getRelations()) {
        if (rel->getRepresentation() == RelationRepresentation::EQREL) {
            relationsToIgnore.insert(rel->getQualifiedName());
        }
    }

    // - Any relation with execution plans
    for (auto* clause : program.getClauses()) {
        if (clause->getExecutionPlan() != nullptr) {
            relationsToIgnore.insert(clause->getHead()->getQualifiedName());
        }
    }

    // - Any atom appearing in a clause containing a counter
    for (auto* clause : program.getClauses()) {
        bool containsCounter = false;
        visitDepthFirst(*clause, [&](const AstCounter& /* counter */) { containsCounter = true; });
        if (containsCounter) {
            visitDepthFirst(
                    *clause, [&](const AstAtom& atom) { relationsToIgnore.insert(atom.getQualifiedName()); });
        }
    }

    return relationsToIgnore;
}

bool MagicSetTransformer::shouldRun(const AstTranslationUnit& tu) {
    const auto& program = *tu.getProgram();
    if (Global::config().has("magic-transform")) return true;
    for (const auto* rel : program.getRelations()) {
        if (rel->hasQualifier(RelationQualifier::MAGIC)) return true;
    }
    return false;
}

bool NormaliseDatabaseTransformer::transform(AstTranslationUnit& translationUnit) {
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

bool NormaliseDatabaseTransformer::partitionIO(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    const auto& ioTypes = *translationUnit.getAnalysis<IOType>();

    // Get all relations that are both input and output
    std::set<AstQualifiedName> relationsToSplit;
    for (auto* rel : program.getRelations()) {
        if (ioTypes.isInput(rel) && (ioTypes.isOutput(rel) || ioTypes.isPrintSize(rel))) {
            relationsToSplit.insert(rel->getQualifiedName());
        }
    }

    // For each of these relations I, add a new relation I' that's input instead.
    // The old relation I is no longer input, but copies over the data from I'.
    for (auto relName : relationsToSplit) {
        const auto* rel = getRelation(program, relName);
        assert(rel != nullptr && "relation does not exist");
        auto newRelName = AstQualifiedName(relName);
        newRelName.prepend("@split_in");

        // Create a new intermediate input relation, I'
        auto newRelation = std::make_unique<AstRelation>(newRelName);
        for (const auto* attr : rel->getAttributes()) {
            newRelation->addAttribute(souffle::clone(attr));
        }

        // Add the rule I <- I'
        auto newClause = std::make_unique<AstClause>();
        auto newHeadAtom = std::make_unique<AstAtom>(relName);
        auto newBodyAtom = std::make_unique<AstAtom>(newRelName);
        for (size_t i = 0; i < rel->getArity(); i++) {
            std::stringstream varName;
            varName << "@var" << i;
            newHeadAtom->addArgument(std::make_unique<AstVariable>(varName.str()));
            newBodyAtom->addArgument(std::make_unique<AstVariable>(varName.str()));
        }
        newClause->setHead(std::move(newHeadAtom));
        newClause->addToBody(std::move(newBodyAtom));

        // New relation I' should be input, original should not
        std::set<const AstIO*> iosToDelete;
        std::set<std::unique_ptr<AstIO>> iosToAdd;
        for (const auto* io : program.getIOs()) {
            if (io->getQualifiedName() == relName && io->getType() == AstIoType::input) {
                // New relation inherits the old input rules
                auto newIO = souffle::clone(io);
                newIO->setQualifiedName(newRelName);
                iosToAdd.insert(std::move(newIO));

                // Original no longer has them
                iosToDelete.insert(io);
            }
        }
        for (const auto* io : iosToDelete) {
            program.removeIO(io);
        }
        for (auto& io : iosToAdd) {
            program.addIO(souffle::clone(io));
        }

        // Add in the new relation and the copy clause
        program.addRelation(std::move(newRelation));
        program.addClause(std::move(newClause));
    }

    return !relationsToSplit.empty();
}

bool NormaliseDatabaseTransformer::extractIDB(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    const auto& ioTypes = *translationUnit.getAnalysis<IOType>();

    // Helper method to check if an input relation has no associated rules
    auto isStrictlyEDB = [&](const AstRelation* rel) {
        bool hasRules = false;
        for (const auto* clause : getClauses(program, rel->getQualifiedName())) {
            visitDepthFirst(clause->getBodyLiterals(), [&](const AstAtom& /* atom */) { hasRules = true; });
        }
        return !hasRules;
    };

    // Get all input relations that also have IDB rules attached
    std::set<AstQualifiedName> inputRelationNames;
    for (auto* rel : program.getRelations()) {
        if (ioTypes.isInput(rel) && !isStrictlyEDB(rel)) {
            assert(!ioTypes.isOutput(rel) && !ioTypes.isPrintSize(rel) &&
                    "input relations should not be output at this stage");
            inputRelationNames.insert(rel->getQualifiedName());
        }
    }

    // Add a new intermediate non-input relation for each
    // These will cover relation appearances in IDB rules
    std::map<AstQualifiedName, AstQualifiedName> inputToIntermediate;
    for (const auto& inputRelationName : inputRelationNames) {
        // Give it a unique name
        AstQualifiedName intermediateName(inputRelationName);
        intermediateName.prepend("@interm_in");
        inputToIntermediate[inputRelationName] = intermediateName;

        // Add the relation
        auto intermediateRelation = souffle::clone(getRelation(program, inputRelationName));
        intermediateRelation->setQualifiedName(intermediateName);
        program.addRelation(std::move(intermediateRelation));
    }

    // Rename them systematically
    renameAtoms(program, inputToIntermediate);

    // Add the rule I' <- I
    for (const auto& inputRelationName : inputRelationNames) {
        auto queryHead = std::make_unique<AstAtom>(inputToIntermediate.at(inputRelationName));
        auto queryLiteral = std::make_unique<AstAtom>(inputRelationName);

        // Give them identical arguments
        const auto* inputRelation = getRelation(program, inputRelationName);
        for (size_t i = 0; i < inputRelation->getArity(); i++) {
            std::stringstream var;
            var << "@query_x" << i;
            queryHead->addArgument(std::make_unique<AstVariable>(var.str()));
            queryLiteral->addArgument(std::make_unique<AstVariable>(var.str()));
        }

        auto query = std::make_unique<AstClause>(std::move(queryHead));
        query->addToBody(std::move(queryLiteral));
        program.addClause(std::move(query));
    }

    return !inputRelationNames.empty();
}

bool NormaliseDatabaseTransformer::querifyOutputRelations(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();

    // Helper method to check if a relation is a single-rule output query
    auto isStrictlyOutput = [&](const AstRelation* rel) {
        bool strictlyOutput = true;
        size_t ruleCount = 0;

        for (const auto* clause : program.getClauses()) {
            // Check if the relation is used in the body of any rules
            visitDepthFirst(clause->getBodyLiterals(), [&](const AstAtom& atom) {
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
    const auto& ioTypes = *translationUnit.getAnalysis<IOType>();
    std::set<AstQualifiedName> outputRelationNames;
    for (auto* rel : program.getRelations()) {
        if ((ioTypes.isOutput(rel) || ioTypes.isPrintSize(rel)) && !isStrictlyOutput(rel)) {
            assert(!ioTypes.isInput(rel) && "output relations should not be input at this stage");
            outputRelationNames.insert(rel->getQualifiedName());
        }
    }

    // Add a new intermediate non-output relation for each
    // These will cover relation appearances in intermediate rules
    std::map<AstQualifiedName, AstQualifiedName> outputToIntermediate;
    for (const auto& outputRelationName : outputRelationNames) {
        // Give it a unique name
        AstQualifiedName intermediateName(outputRelationName);
        intermediateName.prepend("@interm_out");
        outputToIntermediate[outputRelationName] = intermediateName;

        // Add the relation
        auto intermediateRelation = souffle::clone(getRelation(program, outputRelationName));
        intermediateRelation->setQualifiedName(intermediateName);
        program.addRelation(std::move(intermediateRelation));
    }

    // Rename them systematically
    renameAtoms(program, outputToIntermediate);

    // Add the rule I <- I'
    for (const auto& outputRelationName : outputRelationNames) {
        auto queryHead = std::make_unique<AstAtom>(outputRelationName);
        auto queryLiteral = std::make_unique<AstAtom>(outputToIntermediate.at(outputRelationName));

        // Give them identical arguments
        const auto* outputRelation = getRelation(program, outputRelationName);
        for (size_t i = 0; i < outputRelation->getArity(); i++) {
            std::stringstream var;
            var << "@query_x" << i;
            queryHead->addArgument(std::make_unique<AstVariable>(var.str()));
            queryLiteral->addArgument(std::make_unique<AstVariable>(var.str()));
        }
        auto query = std::make_unique<AstClause>(std::move(queryHead));
        query->addToBody(std::move(queryLiteral));
        program.addClause(std::move(query));
    }

    return !outputRelationNames.empty();
}

bool NormaliseDatabaseTransformer::normaliseArguments(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();

    // Replace all non-variable-arguments nested inside the node with named variables
    // Also, keeps track of constraints to add to keep the clause semantically equivalent
    struct argument_normaliser : public AstNodeMapper {
        std::set<std::unique_ptr<AstBinaryConstraint>>& constraints;
        int& changeCount;

        argument_normaliser(std::set<std::unique_ptr<AstBinaryConstraint>>& constraints, int& changeCount)
                : constraints(constraints), changeCount(changeCount) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* aggr = dynamic_cast<AstAggregator*>(node.get())) {
                // Aggregator variable scopes should be maintained, so changes shouldn't propagate
                // above this level.
                std::set<std::unique_ptr<AstBinaryConstraint>> subConstraints;
                argument_normaliser aggrUpdate(subConstraints, changeCount);
                aggr->apply(aggrUpdate);

                // Add the constraints to this level
                std::vector<std::unique_ptr<AstLiteral>> newBodyLiterals;
                for (const auto* lit : aggr->getBodyLiterals()) {
                    newBodyLiterals.push_back(souffle::clone(lit));
                }
                for (auto& constr : subConstraints) {
                    newBodyLiterals.push_back(souffle::clone(constr));
                }

                // Update the node to reflect normalised aggregator
                node = aggr->getTargetExpression() != nullptr
                               ? std::make_unique<AstAggregator>(aggr->getOperator(),
                                         souffle::clone(aggr->getTargetExpression()),
                                         std::move(newBodyLiterals))
                               : std::make_unique<AstAggregator>(
                                         aggr->getOperator(), nullptr, std::move(newBodyLiterals));
            } else {
                // Otherwise, just normalise children as usual.
                node->apply(*this);
            }

            // All non-variables should be normalised
            if (auto* arg = dynamic_cast<AstArgument*>(node.get())) {
                if (dynamic_cast<AstVariable*>(arg) == nullptr) {
                    std::stringstream name;
                    name << "@abdul" << changeCount++;

                    // Unnamed variables don't need a new constraint, just give them a name
                    if (dynamic_cast<AstUnnamedVariable*>(arg) != nullptr) {
                        return std::make_unique<AstVariable>(name.str());
                    }

                    // Link other variables back to their original value with a `<var> = <arg>` constraint
                    constraints.insert(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                            std::make_unique<AstVariable>(name.str()), souffle::clone(arg)));
                    return std::make_unique<AstVariable>(name.str());
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
        std::set<std::unique_ptr<AstBinaryConstraint>> constraintsToAdd;
        argument_normaliser update(constraintsToAdd, changeCount);

        // Apply to each clause head
        clause->getHead()->apply(update);

        // Apply to each body literal that isn't already a `<var> = <arg>` constraint
        for (AstLiteral* lit : clause->getBodyLiterals()) {
            if (auto* bc = dynamic_cast<AstBinaryConstraint*>(lit)) {
                if (bc->getOperator() == BinaryConstraintOp::EQ &&
                        dynamic_cast<AstVariable*>(bc->getLHS()) != nullptr) {
                    continue;
                }
            }
            lit->apply(update);
        }

        // Also apply to each record
        visitDepthFirst(*clause, [&](const AstRecordInit& rec) {
            for (AstArgument* arg : rec.getArguments()) {
                arg->apply(update);
            }
        });

        // Add each necessary new constraint to the clause
        for (auto& constraint : constraintsToAdd) {
            clause->addToBody(souffle::clone(constraint));
        }

        changed |= changeCount != 0;
    }

    return changed;
}

AstQualifiedName AdornDatabaseTransformer::getAdornmentID(
        const AstQualifiedName& relName, const std::string& adornmentMarker) {
    if (adornmentMarker == "") return relName;
    AstQualifiedName adornmentID(relName);
    std::stringstream adornmentMarkerRepr;
    adornmentMarkerRepr << "{" << adornmentMarker << "}";
    adornmentID.append(adornmentMarkerRepr.str());
    return adornmentID;
}

std::unique_ptr<AstClause> AdornDatabaseTransformer::adornClause(
        const AstClause* clause, const std::string& adornmentMarker) {
    const auto& relName = clause->getHead()->getQualifiedName();
    const auto& headArgs = clause->getHead()->getArguments();
    BindingStore variableBindings(clause, adornmentMarker);

    // Create the adorned clause with an empty body
    auto adornedClause = std::make_unique<AstClause>();

    // Copy over plans if needed
    if (clause->getExecutionPlan() != nullptr) {
        assert(contains(relationsToIgnore, clause->getHead()->getQualifiedName()) &&
                "clauses with plans should be ignored");
        adornedClause->setExecutionPlan(souffle::clone(clause->getExecutionPlan()));
    }

    // Create the head atom
    auto adornedHeadAtom = std::make_unique<AstAtom>(getAdornmentID(relName, adornmentMarker));
    assert((adornmentMarker == "" || headArgs.size() == adornmentMarker.length()) &&
            "adornment marker should correspond to head atom variables");
    for (const auto* arg : headArgs) {
        const auto* var = dynamic_cast<const AstVariable*>(arg);
        assert(var != nullptr && "expected only variables in head");
        adornedHeadAtom->addArgument(souffle::clone(var));
    }
    adornedClause->setHead(std::move(adornedHeadAtom));

    // Add in adorned body literals
    std::vector<std::unique_ptr<AstLiteral>> adornedBodyLiterals;
    for (const auto* lit : clause->getBodyLiterals()) {
        if (const auto* negation = dynamic_cast<const AstNegation*>(lit)) {
            // Negated atoms should not be adorned, but their clauses should be anyway
            const auto negatedAtomName = negation->getAtom()->getQualifiedName();
            assert(contains(relationsToIgnore, negatedAtomName) && "negated atoms should not be adorned");
            queueAdornment(negatedAtomName, "");
        }

        if (dynamic_cast<const AstAtom*>(lit) == nullptr) {
            // Non-atoms are added directly
            adornedBodyLiterals.push_back(souffle::clone(lit));
            continue;
        }

        const auto* atom = dynamic_cast<const AstAtom*>(lit);
        assert(atom != nullptr && "expected atom");

        // Form the appropriate adornment marker
        std::stringstream atomAdornment;
        if (!contains(relationsToIgnore, atom->getQualifiedName())) {
            for (const auto* arg : atom->getArguments()) {
                const auto* var = dynamic_cast<const AstVariable*>(arg);
                assert(var != nullptr && "expected only variables in atom");
                atomAdornment << (variableBindings.isBound(var->getName()) ? "b" : "f");
            }
        }
        auto currAtomAdornmentID = getAdornmentID(atom->getQualifiedName(), atomAdornment.str());

        // Add to the ToDo queue if needed
        queueAdornment(atom->getQualifiedName(), atomAdornment.str());

        // Add the adorned version to the clause
        auto adornedBodyAtom = souffle::clone(atom);
        adornedBodyAtom->setQualifiedName(currAtomAdornmentID);
        adornedBodyLiterals.push_back(std::move(adornedBodyAtom));

        // All arguments are now bound
        for (const auto* arg : atom->getArguments()) {
            const auto* var = dynamic_cast<const AstVariable*>(arg);
            assert(var != nullptr && "expected only variables in atom");
            variableBindings.bindVariable(var->getName());
        }
    }
    adornedClause->setBodyLiterals(std::move(adornedBodyLiterals));

    return adornedClause;
}

bool AdornDatabaseTransformer::transform(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    const auto& ioTypes = *translationUnit.getAnalysis<IOType>();

    relationsToIgnore = getIgnoredRelations(translationUnit);

    // Output relations trigger the adornment process
    for (const auto* rel : program.getRelations()) {
        if (ioTypes.isOutput(rel) || ioTypes.isPrintSize(rel)) {
            queueAdornment(rel->getQualifiedName(), "");
        }
    }

    // Keep going while there's things to adorn
    while (hasAdornmentToProcess()) {
        // Pop off the next head adornment to do
        const auto& [relName, adornmentMarker] = nextAdornmentToProcess();

        // Add the adorned relation if needed
        if (adornmentMarker != "") {
            const auto* rel = getRelation(program, relName);
            assert(rel != nullptr && "relation does not exist");

            auto adornedRelation = std::make_unique<AstRelation>(getAdornmentID(relName, adornmentMarker));
            for (const auto* attr : rel->getAttributes()) {
                adornedRelation->addAttribute(souffle::clone(attr));
            }
            program.addRelation(std::move(adornedRelation));
        }

        // Adorn every clause correspondingly
        for (const auto* clause : getClauses(program, relName)) {
            if (adornmentMarker == "") {
                redundantClauses.push_back(souffle::clone(clause));
            }
            auto adornedClause = adornClause(clause, adornmentMarker);
            adornedClauses.push_back(std::move(adornedClause));
        }
    }

    // Swap over the redundant clauses with the adorned clauses
    for (const auto& clause : redundantClauses) {
        program.removeClause(clause.get());
    }

    for (auto& clause : adornedClauses) {
        program.addClause(souffle::clone(clause));
    }

    return !adornedClauses.empty() || !redundantClauses.empty();
}

AstQualifiedName NegativeLabellingTransformer::getNegativeLabel(const AstQualifiedName& name) {
    AstQualifiedName newName(name);
    newName.prepend("@neglabel");
    return newName;
}

AstQualifiedName PositiveLabellingTransformer::getPositiveLabel(const AstQualifiedName& name, size_t count) {
    std::stringstream label;
    label << "@poscopy_" << count;
    AstQualifiedName labelledName(name);
    labelledName.prepend(label.str());
    return labelledName;
}

bool LabelDatabaseTransformer::isNegativelyLabelled(const AstQualifiedName& name) {
    auto qualifiers = name.getQualifiers();
    assert(!qualifiers.empty() && "unexpected empty qualifier list");
    return qualifiers[0] == "@neglabel";
}

bool NegativeLabellingTransformer::transform(AstTranslationUnit& translationUnit) {
    const auto& sccGraph = *translationUnit.getAnalysis<SCCGraphAnalysis>();
    auto& program = *translationUnit.getProgram();

    std::set<AstQualifiedName> relationsToLabel;
    std::set<std::unique_ptr<AstClause>> clausesToAdd;
    auto ignoredRelations = getIgnoredRelations(translationUnit);

    // Negatively label all relations that might affect stratification after MST
    //      - Negated relations
    //      - Relations that appear in aggregators
    visitDepthFirst(program, [&](const AstNegation& neg) {
        auto* atom = neg.getAtom();
        auto relName = atom->getQualifiedName();
        if (contains(ignoredRelations, relName)) return;
        atom->setQualifiedName(getNegativeLabel(relName));
        relationsToLabel.insert(relName);
    });
    visitDepthFirst(program, [&](const AstAggregator& aggr) {
        visitDepthFirst(aggr, [&](const AstAtom& atom) {
            auto relName = atom.getQualifiedName();
            if (contains(ignoredRelations, relName)) return;
            const_cast<AstAtom&>(atom).setQualifiedName(getNegativeLabel(relName));
            relationsToLabel.insert(relName);
        });
    });

    // Copy over the rules for labelled relations one stratum at a time
    for (size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        // Check which relations to label in this stratum
        const auto& stratumRels = sccGraph.getInternalRelations(stratum);
        std::map<AstQualifiedName, AstQualifiedName> newSccFriendNames;
        for (const auto* rel : stratumRels) {
            auto relName = rel->getQualifiedName();
            if (contains(ignoredRelations, relName)) continue;
            relationsToLabel.insert(relName);
            newSccFriendNames[relName] = getNegativeLabel(relName);
        }

        // Negatively label the relations in a new copy of this stratum
        for (const auto* rel : stratumRels) {
            for (auto* clause : getClauses(program, rel->getQualifiedName())) {
                auto neggedClause = souffle::clone(clause);
                renameAtoms(*neggedClause, newSccFriendNames);
                clausesToAdd.insert(std::move(neggedClause));
            }
        }
    }

    // Add in all the relations that were labelled
    for (const auto& relName : relationsToLabel) {
        const auto* originalRel = getRelation(program, relName);
        assert(originalRel != nullptr && "unlabelled relation does not exist");
        auto labelledRelation = souffle::clone(originalRel);
        labelledRelation->setQualifiedName(getNegativeLabel(relName));
        program.addRelation(std::move(labelledRelation));
    }

    // Add in all the negged clauses
    for (const auto& clause : clausesToAdd) {
        program.addClause(souffle::clone(clause));
    }

    return !relationsToLabel.empty();
}

bool PositiveLabellingTransformer::transform(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    const auto& sccGraph = *translationUnit.getAnalysis<SCCGraphAnalysis>();
    const auto& precedenceGraph = translationUnit.getAnalysis<PrecedenceGraphAnalysis>()->graph();
    auto ignoredRelations = getIgnoredRelations(translationUnit);

    // Partition the strata into neglabelled and regular
    std::set<size_t> neglabelledStrata;
    std::map<size_t, size_t> originalStrataCopyCount;
    for (size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        size_t numNeggedRelations = 0;
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
    std::map<size_t, std::set<size_t>> dependentStrata;
    for (size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        dependentStrata[stratum] = std::set<size_t>();
    }
    for (const auto* rel : program.getRelations()) {
        size_t stratum = sccGraph.getSCC(rel);
        precedenceGraph.visitDepthFirst(rel, [&](const auto* dependentRel) {
            dependentStrata[stratum].insert(sccGraph.getSCC(dependentRel));
        });
    }

    // Label the positive derived literals in the clauses of neglabelled relations
    // Need a new copy of those relations up to that point for each
    for (size_t stratum = 0; stratum < sccGraph.getNumberOfSCCs(); stratum++) {
        if (!contains(neglabelledStrata, stratum)) continue;

        // Rename in the current stratum
        for (const auto* rel : sccGraph.getInternalRelations(stratum)) {
            assert(isNegativelyLabelled(rel->getQualifiedName()) &&
                    "should only be looking at neglabelled strata");
            const auto& clauses = getClauses(program, *rel);
            std::set<AstQualifiedName> relsToCopy;

            // Get the unignored unlabelled relations appearing in the rules
            for (const auto* clause : clauses) {
                visitDepthFirst(*clause, [&](const AstAtom& atom) {
                    const auto& name = atom.getQualifiedName();
                    if (!contains(ignoredRelations, name) && !isNegativelyLabelled(name)) {
                        relsToCopy.insert(name);
                    }
                });
            }

            // Positively label them
            for (auto* clause : clauses) {
                std::map<AstQualifiedName, AstQualifiedName> labelledNames;
                for (const auto& relName : relsToCopy) {
                    size_t relStratum = sccGraph.getSCC(getRelation(program, relName));
                    size_t copyCount = originalStrataCopyCount.at(relStratum) + 1;
                    labelledNames[relName] = getPositiveLabel(relName, copyCount);
                }
                renameAtoms(*clause, labelledNames);
            }
        }

        // Create the rules (from all previous strata) for the newly positive labelled literals
        for (int preStratum = stratum - 1; preStratum >= 0; preStratum--) {
            if (contains(neglabelledStrata, preStratum)) continue;
            if (!contains(dependentStrata[preStratum], stratum)) continue;

            for (const auto* rel : sccGraph.getInternalRelations(preStratum)) {
                if (contains(ignoredRelations, rel->getQualifiedName())) continue;

                for (const auto* clause : getClauses(program, rel->getQualifiedName())) {
                    // Grab the new names for all unignored unlabelled positive atoms
                    std::map<AstQualifiedName, AstQualifiedName> labelledNames;
                    visitDepthFirst(*clause, [&](const AstAtom& atom) {
                        const auto& relName = atom.getQualifiedName();
                        if (contains(ignoredRelations, relName) || isNegativelyLabelled(relName)) return;
                        size_t relStratum = sccGraph.getSCC(getRelation(program, relName));
                        size_t copyCount = originalStrataCopyCount.at(relStratum) + 1;
                        labelledNames[relName] = getPositiveLabel(relName, copyCount);
                    });

                    // Rename atoms accordingly
                    auto labelledClause = souffle::clone(clause);
                    renameAtoms(*labelledClause, labelledNames);
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
        for (size_t copy = 0; copy < numCopies; copy++) {
            for (auto* rel : stratumRels) {
                auto newRelation = souffle::clone(rel);
                newRelation->setQualifiedName(getPositiveLabel(newRelation->getQualifiedName(), copy + 1));
                program.addRelation(std::move(newRelation));
                changed = true;
            }
        }
    }
    return changed;
}

bool MagicSetCoreTransformer::isAdorned(const AstQualifiedName& name) {
    // Grab the final qualifier - this is where the adornment is if it exists
    auto qualifiers = name.getQualifiers();
    assert(!qualifiers.empty() && "unexpected empty qualifier list");
    auto finalQualifier = qualifiers[qualifiers.size() - 1];
    assert(finalQualifier.length() > 0 && "unexpected empty qualifier");

    // Pattern: {[bf]*}
    if (finalQualifier[0] == '{' && finalQualifier[finalQualifier.length() - 1] == '}') {
        for (size_t i = 1; i < finalQualifier.length() - 1; i++) {
            char curBindingType = finalQualifier[i];
            if (curBindingType != 'b' && curBindingType != 'f') {
                return false;
            }
        }
        return true;
    }
    return false;
}

std::string MagicSetCoreTransformer::getAdornment(const AstQualifiedName& name) {
    assert(isAdorned(name) && "relation not adorned");
    auto qualifiers = name.getQualifiers();
    auto finalQualifier = qualifiers[qualifiers.size() - 1];
    std::stringstream binding;
    for (size_t i = 1; i < finalQualifier.length() - 1; i++) {
        binding << finalQualifier[i];
    }
    return binding.str();
}

AstQualifiedName MagicSetCoreTransformer::getMagicName(const AstQualifiedName& name) {
    assert(isAdorned(name) && "cannot magify unadorned predicates");
    AstQualifiedName magicRelName(name);
    magicRelName.prepend("@magic");
    return magicRelName;
}

std::unique_ptr<AstAtom> MagicSetCoreTransformer::createMagicAtom(const AstAtom* atom) {
    auto origRelName = atom->getQualifiedName();
    auto args = atom->getArguments();

    auto magicAtom = std::make_unique<AstAtom>(getMagicName(origRelName));

    auto adornmentMarker = getAdornment(origRelName);
    for (size_t i = 0; i < args.size(); i++) {
        if (adornmentMarker[i] == 'b') {
            magicAtom->addArgument(souffle::clone(args[i]));
        }
    }

    return magicAtom;
}

void MagicSetCoreTransformer::addRelevantVariables(
        std::set<std::string>& variables, const std::vector<const AstBinaryConstraint*> eqConstraints) {
    // Helper method to check if all variables in an argument are bound
    auto isFullyBound = [&](const AstArgument* arg) {
        bool fullyBound = true;
        visitDepthFirst(
                *arg, [&](const AstVariable& var) { fullyBound &= contains(variables, var.getName()); });
        return fullyBound;
    };

    // Helper method to add all newly relevant variables given a lhs = rhs constraint
    auto addLocallyRelevantVariables = [&](const AstArgument* lhs, const AstArgument* rhs) {
        const auto* lhsVar = dynamic_cast<const AstVariable*>(lhs);
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
        if (const auto* rhsRec = dynamic_cast<const AstRecordInit*>(rhs)) {
            for (const auto* arg : rhsRec->getArguments()) {
                const auto* subVar = dynamic_cast<const AstVariable*>(arg);
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
            assert(eqConstraint->getOperator() == BinaryConstraintOp::EQ && "expected only eq constraints");
            fixpointReached &= addLocallyRelevantVariables(eqConstraint->getLHS(), eqConstraint->getRHS());
            fixpointReached &= addLocallyRelevantVariables(eqConstraint->getRHS(), eqConstraint->getLHS());
        }
    }
}

std::unique_ptr<AstClause> MagicSetCoreTransformer::createMagicClause(const AstAtom* atom,
        const std::vector<std::unique_ptr<AstAtom>>& constrainingAtoms,
        const std::vector<const AstBinaryConstraint*> eqConstraints) {
    auto magicHead = createMagicAtom(atom);
    auto magicClause = std::make_unique<AstClause>();

    // Add in all constraining atoms
    for (const auto& bindingAtom : constrainingAtoms) {
        magicClause->addToBody(souffle::clone(bindingAtom));
    }

    // Get the set of all variables that will be relevant to the magic clause
    std::set<std::string> relevantVariables;
    visitDepthFirst(
            constrainingAtoms, [&](const AstVariable& var) { relevantVariables.insert(var.getName()); });
    visitDepthFirst(*magicHead, [&](const AstVariable& var) { relevantVariables.insert(var.getName()); });
    addRelevantVariables(relevantVariables, eqConstraints);

    // Add in all eq constraints containing ONLY relevant variables
    for (const auto* eqConstraint : eqConstraints) {
        bool addConstraint = true;
        visitDepthFirst(*eqConstraint, [&](const AstVariable& var) {
            if (!contains(relevantVariables, var.getName())) {
                addConstraint = false;
            }
        });

        if (addConstraint) magicClause->addToBody(souffle::clone(eqConstraint));
    }

    magicClause->setHead(std::move(magicHead));
    return magicClause;
}

std::vector<const AstBinaryConstraint*> MagicSetCoreTransformer::getBindingEqualityConstraints(
        const AstClause* clause) {
    std::vector<const AstBinaryConstraint*> equalityConstraints;
    for (const auto* lit : clause->getBodyLiterals()) {
        const auto* bc = dynamic_cast<const AstBinaryConstraint*>(lit);
        if (bc == nullptr || bc->getOperator() != BinaryConstraintOp::EQ) continue;
        if (dynamic_cast<AstVariable*>(bc->getLHS()) != nullptr ||
                dynamic_cast<AstConstant*>(bc->getRHS()) != nullptr) {
            bool containsAggrs = false;
            visitDepthFirst(*bc, [&](const AstAggregator& /* aggr */) { containsAggrs = true; });
            if (!containsAggrs) {
                equalityConstraints.push_back(bc);
            }
        }
    }
    return equalityConstraints;
}

bool MagicSetCoreTransformer::transform(AstTranslationUnit& translationUnit) {
    auto& program = *translationUnit.getProgram();
    std::set<std::unique_ptr<AstClause>> clausesToRemove;
    std::set<std::unique_ptr<AstClause>> clausesToAdd;

    /** Perform the Magic Set Transformation */
    for (const auto* clause : program.getClauses()) {
        clausesToRemove.insert(souffle::clone(clause));

        const auto* head = clause->getHead();
        auto relName = head->getQualifiedName();

        // (1) Add the refined clause
        if (!isAdorned(relName)) {
            // Unadorned relations need not be refined, as every possible tuple is relevant
            clausesToAdd.insert(souffle::clone(clause));
        } else {
            // Refine the clause with a prepended magic atom
            auto magicAtom = createMagicAtom(head);
            auto refinedClause = std::make_unique<AstClause>();
            refinedClause->setHead(souffle::clone(head));
            refinedClause->addToBody(souffle::clone(magicAtom));
            for (auto* literal : clause->getBodyLiterals()) {
                refinedClause->addToBody(souffle::clone(literal));
            }
            clausesToAdd.insert(std::move(refinedClause));
        }

        // (2) Add the associated magic rules
        std::vector<const AstBinaryConstraint*> eqConstraints = getBindingEqualityConstraints(clause);
        std::vector<std::unique_ptr<AstAtom>> atomsToTheLeft;
        if (isAdorned(relName)) {
            // Add the specialising head atom
            // Output relations are not specialised, and so the head will not contribute to specialisation
            atomsToTheLeft.push_back(createMagicAtom(clause->getHead()));
        }
        for (const auto* lit : clause->getBodyLiterals()) {
            const auto* atom = dynamic_cast<const AstAtom*>(lit);
            if (atom == nullptr) continue;
            if (!isAdorned(atom->getQualifiedName())) {
                atomsToTheLeft.push_back(souffle::clone(atom));
                continue;
            }

            // Need to create a magic rule
            auto magicClause = createMagicClause(atom, atomsToTheLeft, eqConstraints);
            atomsToTheLeft.push_back(souffle::clone(atom));
            clausesToAdd.insert(std::move(magicClause));
        }
    }

    for (auto& clause : clausesToAdd) {
        program.addClause(souffle::clone(clause));
    }
    for (const auto& clause : clausesToRemove) {
        program.removeClause(clause.get());
    }

    // Add in the magic relations
    bool changed = false;
    for (const auto* rel : program.getRelations()) {
        const auto& origName = rel->getQualifiedName();
        if (!isAdorned(origName)) continue;
        auto magicRelation = std::make_unique<AstRelation>(getMagicName(origName));
        auto attributes = getRelation(program, origName)->getAttributes();
        auto adornmentMarker = getAdornment(origName);
        for (size_t i = 0; i < attributes.size(); i++) {
            if (adornmentMarker[i] == 'b') {
                magicRelation->addAttribute(souffle::clone(attributes[i]));
            }
        }
        changed = true;
        program.addRelation(std::move(magicRelation));
    }
    return changed;
}

BindingStore::BindingStore(const AstClause* clause, const std::string& adornmentMarker) {
    /* Note that variables can be bound through:
     *  (1) an appearance in a body atom
     *  (2) an appearance in a bound field of the head atom
     *  (3) equality with a fully bound functor (via this class)
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
     */

    // Bind variables in the head that are marked as bound args
    const auto& headArgs = clause->getHead()->getArguments();
    for (size_t i = 0; i < adornmentMarker.length(); i++) {
        const auto* var = dynamic_cast<AstVariable*>(headArgs[i]);
        assert(var != nullptr && "expected only variables in head");
        if (adornmentMarker[i] == 'b') {
            boundHeadVariables.insert(var->getName());
        }
    }

    // Check through for variables bound in the body by a '<var> = <const>' term
    visitDepthFirst(*clause, [&](const AstBinaryConstraint& constr) {
        if (constr.getOperator() == BinaryConstraintOp::EQ && dynamic_cast<AstVariable*>(constr.getLHS()) &&
                dynamic_cast<AstConstant*>(constr.getRHS())) {
            const auto* var = dynamic_cast<AstVariable*>(constr.getLHS());
            bindVariable(var->getName());
        }
    });

    generateBindingDependencies(clause);
    reduceDependencies();
}

void BindingStore::processEqualityBindings(const AstArgument* lhs, const AstArgument* rhs) {
    // Only care about equalities affecting the bound status of variables
    const auto* var = dynamic_cast<const AstVariable*>(lhs);
    if (var == nullptr) return;

    // If all variables on the rhs are bound, then lhs is also bound
    BindingStore::ConjBindingSet depSet;
    visitDepthFirst(*rhs, [&](const AstVariable& subVar) { depSet.insert(subVar.getName()); });
    addBindingDependency(var->getName(), depSet);

    // If the lhs is bound, then all args in the rec on the rhs are also bound
    if (const auto* rec = dynamic_cast<const AstRecordInit*>(rhs)) {
        for (const auto* arg : rec->getArguments()) {
            const auto* subVar = dynamic_cast<const AstVariable*>(arg);
            assert(subVar != nullptr && "expected args to be variables");
            addBindingDependency(subVar->getName(), BindingStore::ConjBindingSet({var->getName()}));
        }
    }
}

void BindingStore::generateBindingDependencies(const AstClause* clause) {
    // Grab all relevant constraints (i.e. eq. constrs not involving aggregators)
    std::set<const AstBinaryConstraint*> constraints;
    visitDepthFirst(*clause, [&](const AstBinaryConstraint& bc) {
        bool containsAggregators = false;
        visitDepthFirst(bc, [&](const AstAggregator& /* aggr */) { containsAggregators = true; });
        if (!containsAggregators && bc.getOperator() == BinaryConstraintOp::EQ) {
            constraints.insert(&bc);
        }
    });

    // Add variable binding dependencies implied by the constraint
    for (const auto* bc : constraints) {
        processEqualityBindings(bc->getLHS(), bc->getRHS());
        processEqualityBindings(bc->getRHS(), bc->getLHS());
    }
}

BindingStore::ConjBindingSet BindingStore::reduceDependency(
        const BindingStore::ConjBindingSet& origDependency) {
    BindingStore::ConjBindingSet newDependency;
    for (const auto& var : origDependency) {
        // Only keep unbound variables in the dependency
        if (!contains(boundVariables, var)) {
            newDependency.insert(var);
        }
    }
    return newDependency;
}

BindingStore::DisjBindingSet BindingStore::reduceDependency(
        const BindingStore::DisjBindingSet& origDependency) {
    BindingStore::DisjBindingSet newDependencies;
    for (const auto& dep : origDependency) {
        auto newDep = reduceDependency(dep);
        if (!newDep.empty()) {
            newDependencies.insert(newDep);
        }
    }
    return newDependencies;
}

bool BindingStore::reduceDependencies() {
    bool changed = false;
    std::map<std::string, BindingStore::DisjBindingSet> newVariableDependencies;
    std::set<std::string> variablesToBind;

    // Reduce each variable's set of dependencies one by one
    for (const auto& [headVar, dependencies] : variableDependencies) {
        // No need to track the dependencies of already-bound variables
        if (contains(boundVariables, headVar)) {
            changed = true;
            continue;
        }

        // Reduce the dependency set based on bound variables
        auto newDependencies = reduceDependency(dependencies);
        if (newDependencies.empty() || newDependencies.size() < dependencies.size()) {
            // At least one dependency has been satisfied, so variable is now bound
            changed = true;
            variablesToBind.insert(headVar);
            continue;
        }
        newVariableDependencies[headVar] = newDependencies;
        changed |= (newDependencies != dependencies);
    }

    // Bind variables that need to be bound
    for (auto var : variablesToBind) {
        boundVariables.insert(var);
    }

    // Repeat it recursively if any changes happened, until we reach a fixpoint
    if (changed) {
        variableDependencies = newVariableDependencies;
        reduceDependencies();
        return true;
    }
    assert(variableDependencies == newVariableDependencies && "unexpected change");
    return false;
}

}  // namespace souffle
