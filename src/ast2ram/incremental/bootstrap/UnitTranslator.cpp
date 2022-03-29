/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnitTranslator.cpp
 *
 ***********************************************************************/

#include "ast2ram/incremental/bootstrap/UnitTranslator.h"
#include "ast2ram/incremental/update/TranslationStrategy.h"
#include "ast2ram/incremental/update/UnitTranslator.h"
#include "ast2ram/incremental/Utils.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Relation.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/DebugInfo.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/Insert.h"
#include "ram/LogRelationTimer.h"
#include "ram/MergeExtend.h"
#include "ram/Negation.h"
#include "ram/Query.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/StringConstant.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/Swap.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "souffle/SymbolTable.h"
#include "souffle/utility/StringUtil.h"
#include <sstream>

namespace souffle::ast2ram::incremental::bootstrap {

Own<ram::Sequence> UnitTranslator::generateProgram(const ast::TranslationUnit& translationUnit) {
    // Do the regular translation
    auto ramProgram = seminaive::UnitTranslator::generateProgram(translationUnit);

    // Add cleanup sequence after evaluation
    auto cleanupMerges = generateCleanupMerges(context->getProgram()->getRelations());
    ramProgram = mk<ram::Sequence>(std::move(ramProgram), std::move(cleanupMerges));

    // Add an update subroutine for incremental updates
    auto updateTranslatorStrategy = mk<ast2ram::TranslationStrategy, incremental::update::TranslationStrategy>();
    auto updateTranslator = Own<ast2ram::UnitTranslator>(updateTranslatorStrategy->createUnitTranslator());

    if (auto* updateUnitTranslator = as<incremental::update::UnitTranslator>(updateTranslator)) {
        addRamSubroutine("update", updateUnitTranslator->generateProgram(translationUnit));

        // updateUnitTranslator->generateProgram also creates subroutines for
        // incrementally updating each stratum, add these to the main program
        for (const auto& sub : updateUnitTranslator->getRamSubroutines()) {
            addRamSubroutine(sub.first, Own<ram::Statement>(clone(sub.second)));
            // addRamSubroutine(sub.first, std::move(sub.second));
        }
    } else {
        std::cerr << "update translator isn't an incremental update translator???" << std::endl;
    }

    std::cout << "hello i am generating an incremental program" << std::endl;
    std::cout << *ramProgram << std::endl;

    return ramProgram;
}

Own<ram::Relation> UnitTranslator::createRamRelation(
        const ast::Relation* baseRelation, std::string ramRelationName) const {
    auto arity = baseRelation->getArity();
    // auto representation = baseRelation->getRepresentation();
    auto representation = RelationRepresentation::INCREMENTAL;

    // Add in base relation information
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    for (const auto& attribute : baseRelation->getAttributes()) {
        attributeNames.push_back(attribute->getName());
        attributeTypeQualifiers.push_back(context->getAttributeTypeQualifier(attribute->getTypeName()));
    }

    // Add in provenance information
    attributeNames.push_back("@iteration");
    attributeTypeQualifiers.push_back("i:number");

    attributeNames.push_back("@count");
    attributeTypeQualifiers.push_back("i:number");

    return mk<ram::Relation>(
            ramRelationName, arity + 2, 2, attributeNames, attributeTypeQualifiers, representation);
}

VecOwn<ram::Relation> UnitTranslator::createRamRelations(const std::vector<std::size_t>& sccOrdering) const {
    // Regular relations
    auto ramRelations = seminaive::UnitTranslator::createRamRelations(sccOrdering);

    // We need to also generate all the auxiliary relations:
    // - prev
    // - delta_prev
    // - new_prev
    //      - if we keep the normal relation as the updated one, the instead of diff_applied,
    //        we can have prev
    // - temp_diff_plus (updated_diff_plus)
    // - temp_diff_minus (updated_diff_minus)
    //      - check if we can simulate semantics of temp without having extra relations
    //        this might be slow, but do it as an initial step
    // - diff_plus
    // - diff_minus
    // - delta_diff_plus
    // - delta_diff_minus
    // - new_diff_plus
    // - new_diff_minus

    for (const auto& scc : sccOrdering) {
        bool isRecursive = context->isRecursiveSCC(scc);
        for (const auto& rel : context->getRelationsInSCC(scc)) {
            // Add diff plus and diff minus relations for incremental eval
            std::string diffPlusName = getDiffPlusRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, diffPlusName));

            std::string diffMinusName = getDiffMinusRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, diffMinusName));

            // Add prev version of the relation used to store the state of the
            // relation before an incremental update
            std::string prevName = getPrevRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, prevName));

            // Add delta versions of the above
            std::string actualDiffPlusName = getActualDiffPlusRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, actualDiffPlusName));

            std::string actualDiffMinusName = getActualDiffMinusRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, actualDiffMinusName));

            // Recursive relations also require @delta and @new variants, with the same signature
            if (isRecursive) {
                // Add new versions of the above
                std::string newDiffPlusName = getNewDiffPlusRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newDiffPlusName));

                std::string newDiffMinusName = getNewDiffMinusRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newDiffMinusName));

                // Add updated versions of the above
                std::string updatedDiffPlusName = getUpdatedDiffPlusRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, updatedDiffPlusName));

                std::string updatedDiffMinusName = getUpdatedDiffMinusRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, updatedDiffMinusName));

                std::string newPrevName = getNewPrevRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newPrevName));

                // For incremental eval, deltas are not required, and are
                // replaced with an indexed constraint on the iteration number
                // annotation
            }
        }
    }

    return ramRelations;
}

void UnitTranslator::addAuxiliaryArity(
        const ast::Relation* /* relation */, std::map<std::string, std::string>& directives) const {
    // directives.insert(std::make_pair("auxArity", "2"));
    directives.insert(std::make_pair("auxValues", "0,1"));
}

Own<ram::Statement> UnitTranslator::generateClearExpiredRelations(
        const std::set<const ast::Relation*>& /* expiredRelations */) const {
    // Relations should be preserved if provenance is enabled
    return mk<ram::Sequence>();
}

Own<ram::Statement> UnitTranslator::generateStratumTableUpdates(
        const std::set<const ast::Relation*>& scc) const {
    VecOwn<ram::Statement> updateTable;

    for (const ast::Relation* rel : scc) {
        // Copy @new into main relation, @delta := @new, and empty out @new
        std::string mainRelation = getConcreteRelationName(rel->getQualifiedName());
        std::string newRelation = getNewRelationName(rel->getQualifiedName());
        std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());

        // swap new and and delta relation and clear new relation afterwards (if not a subsumptive relation)
        Own<ram::Statement> updateRelTable;
        if (!context->hasSubsumptiveClause(rel->getQualifiedName())) {
            updateRelTable = mk<ram::Sequence>(mk<ram::Clear>(deltaRelation),
                    generateMergeRelationsWithFilter(rel, deltaRelation, newRelation, mainRelation),
                    generateMergeRelations(rel, mainRelation, newRelation),
                    mk<ram::Clear>(newRelation));

        /* TODO: Handle subsumptive clauses correctly
        } else {
            updateRelTable = generateMergeRelations(rel, mainRelation, deltaRelation);
            */
        }

        // Measure update time
        if (Global::config().has("profile")) {
            updateRelTable = mk<ram::LogRelationTimer>(std::move(updateRelTable),
                    LogStatement::cRecursiveRelation(toString(rel->getQualifiedName()), rel->getSrcLoc()),
                    newRelation);
        }

        appendStmt(updateTable, std::move(updateRelTable));
    }
    return mk<ram::Sequence>(std::move(updateTable));
}

Own<ram::Statement> UnitTranslator::generateMergeRelations(
        const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const {
    VecOwn<ram::Expression> values;

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity() + 2; i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
    }

    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(insertion)));
    return stmt;
}

Own<ram::Statement> UnitTranslator::generateMergeRelationsWithFilter(const ast::Relation* rel,
        const std::string& destRelation, const std::string& srcRelation,
        const std::string& filterRelation) const {
    VecOwn<ram::Expression> values;
    VecOwn<ram::Expression> values2;

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity() + 2; i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
        if (i < rel->getArity()) {
            values2.push_back(mk<ram::TupleElement>(0, i));
        } else {
            values2.push_back(mk<ram::UndefValue>());
        }
    }
    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto filtered =
            mk<ram::Filter>(mk<ram::Negation>(mk<ram::ExistenceCheck>(filterRelation, std::move(values2))),
                    std::move(insertion));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(filtered)));
    return stmt;
}

Own<ram::Statement> UnitTranslator::generateCleanupMerges(const std::vector<ast::Relation*>& rels) const {
    VecOwn<ram::Statement> cleanupSequence;
    for (auto rel : rels) {
        appendStmt(cleanupSequence, generateMergeRelations(rel, getPrevRelationName(rel->getQualifiedName()), getRelationName(rel->getQualifiedName())));
    }
    return mk<ram::Sequence>(std::move(cleanupSequence));
}

Own<ram::SubroutineReturn> UnitTranslator::makeRamReturnTrue() const {
    VecOwn<ram::Expression> returnTrue;
    returnTrue.push_back(mk<ram::SignedConstant>(1));
    return mk<ram::SubroutineReturn>(std::move(returnTrue));
}

Own<ram::SubroutineReturn> UnitTranslator::makeRamReturnFalse() const {
    VecOwn<ram::Expression> returnFalse;
    returnFalse.push_back(mk<ram::SignedConstant>(0));
    return mk<ram::SubroutineReturn>(std::move(returnFalse));
}

Own<ram::Sequence> UnitTranslator::makeIfStatement(
        Own<ram::Condition> condition, Own<ram::Operation> trueOp, Own<ram::Operation> falseOp) const {
    auto negatedCondition = mk<ram::Negation>(clone(condition));

    auto trueBranch = mk<ram::Query>(mk<ram::Filter>(std::move(condition), std::move(trueOp)));
    auto falseBranch = mk<ram::Query>(mk<ram::Filter>(std::move(negatedCondition), std::move(falseOp)));

    return mk<ram::Sequence>(std::move(trueBranch), std::move(falseBranch));
}

}  // namespace souffle::ast2ram::incremental::bootstrap
