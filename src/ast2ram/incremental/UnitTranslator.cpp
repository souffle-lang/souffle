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

#include "ast2ram/incremental/UnitTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/Call.h"
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
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "souffle/SymbolTable.h"
#include "souffle/utility/StringUtil.h"
#include <sstream>

namespace souffle::ast2ram::incremental {

Own<ram::Sequence> UnitTranslator::generateProgram(const ast::TranslationUnit& translationUnit) {
    // Do the regular translation
    auto ramProgram = seminaive::UnitTranslator::generateProgram(translationUnit);

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
    // - diff_applied
    // - delta_diff_applied
    // - new_diff_applied
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

    return ramRelations;
}

void UnitTranslator::addAuxiliaryArity(
        const ast::Relation* /* relation */, std::map<std::string, std::string>& directives) const {
    directives.insert(std::make_pair("auxArity", "2"));
}

Own<ram::Statement> UnitTranslator::generateClearExpiredRelations(
        const std::set<const ast::Relation*>& /* expiredRelations */) const {
    // Relations should be preserved if provenance is enabled
    return mk<ram::Sequence>();
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
    if (rel->getRepresentation() == RelationRepresentation::EQREL) {
        return mk<ram::Sequence>(mk<ram::MergeExtend>(destRelation, srcRelation), std::move(stmt));
    }
    return stmt;
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

}  // namespace souffle::ast2ram::incremental
