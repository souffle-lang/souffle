/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslatorContext.cpp
 *
 ***********************************************************************/

#include "ast2ram/utility/TranslatorContext.h"
#include "Global.h"
#include "ast/Atom.h"
#include "ast/BranchInit.h"
#include "ast/Directive.h"
#include "ast/QualifiedName.h"
#include "ast/SubsumptiveClause.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/Functor.h"
#include "ast/analysis/IOType.h"
#include "ast/analysis/RecursiveClauses.h"
#include "ast/analysis/RelationSchedule.h"
#include "ast/analysis/SCCGraph.h"
#include "ast/analysis/UniqueKeys.h"
#include "ast/analysis/typesystem/PolymorphicObjects.h"
#include "ast/analysis/typesystem/SumTypeBranches.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/analysis/typesystem/TypeEnvironment.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "ast/utility/SipsMetric.h"
#include "ast/utility/Utils.h"
#include "ast2ram/ClauseTranslator.h"
#include "ast2ram/ConstraintTranslator.h"
#include "ast2ram/ValueTranslator.h"
#include "ast2ram/incremental/bootstrap/TranslationStrategy.h"
#include "ast2ram/incremental/update/TranslationStrategy.h"
#include "ast2ram/incremental/update/ClauseTranslator.h"
#include "ast2ram/provenance/TranslationStrategy.h"
#include "ast2ram/seminaive/TranslationStrategy.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Statement.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/StringUtil.h"
#include <set>

namespace souffle::ast2ram {

TranslatorContext::TranslatorContext(const ast::TranslationUnit& tu, bool isIncrementalUpdate) {
    program = &tu.getProgram();

    // Set up analyses
    functorAnalysis = &tu.getAnalysis<ast::analysis::FunctorAnalysis>();
    recursiveClauses = &tu.getAnalysis<ast::analysis::RecursiveClausesAnalysis>();
    sccGraph = &tu.getAnalysis<ast::analysis::SCCGraphAnalysis>();
    relationSchedule = &tu.getAnalysis<ast::analysis::RelationScheduleAnalysis>();
    ioType = &tu.getAnalysis<ast::analysis::IOTypeAnalysis>();
    typeAnalysis = &tu.getAnalysis<ast::analysis::TypeAnalysis>();
    typeEnv = &tu.getAnalysis<ast::analysis::TypeEnvironmentAnalysis>().getTypeEnvironment();
    sumTypeBranches = &tu.getAnalysis<ast::analysis::SumTypeBranchesAnalysis>();
    polyAnalysis = &tu.getAnalysis<ast::analysis::PolymorphicObjectsAnalysis>();
    uniqueKeysAnalysis = &tu.getAnalysis<ast::analysis::UniqueKeysAnalysis>();

    // Set up clause nums
    for (const ast::Relation* rel : program->getRelations()) {
        std::size_t count = 1;
        for (auto&& clause : program->getClauses(*rel)) {
            if (isFact(*clause)) {
                clauseNums[clause] = 0;
            }
            clauseNums[clause] = count++;
        }
    }

    // Set up SIPS metric
    std::string sipsChosen = "all-bound";
    if (Global::config().has("RamSIPS")) {
        sipsChosen = Global::config().get("RamSIPS");
    }
    sipsMetric = ast::SipsMetric::create(sipsChosen, tu);

    // Set up the correct strategy
    if (Global::config().has("provenance")) {
        translationStrategy = mk<provenance::TranslationStrategy>();
    } else if (Global::config().has("incremental")) {
        if (isIncrementalUpdate) {
            translationStrategy = mk<incremental::update::TranslationStrategy>();
        } else {
            translationStrategy = mk<incremental::bootstrap::TranslationStrategy>();
        }
    } else {
        translationStrategy = mk<seminaive::TranslationStrategy>();
    }
}

TranslatorContext::~TranslatorContext() = default;

bool TranslatorContext::isRecursiveClause(const ast::Clause* clause) const {
    return recursiveClauses->recursive(clause);
}

std::size_t TranslatorContext::getClauseNum(const ast::Clause* clause) const {
    assert(contains(clauseNums, clause) && "clause num should exist for all clauses");
    return clauseNums.at(clause);
}

std::string TranslatorContext::getAttributeTypeQualifier(const ast::QualifiedName& name) const {
    return getTypeQualifier(typeEnv->getType(name));
}

std::size_t TranslatorContext::getNumberOfSCCs() const {
    return sccGraph->getNumberOfSCCs();
}

bool TranslatorContext::isRecursiveSCC(std::size_t scc) const {
    return sccGraph->isRecursive(scc);
}

std::vector<ast::Directive*> TranslatorContext::getStoreDirectives(const ast::QualifiedName& name) const {
    return filter(program->getDirectives(name), [&](const ast::Directive* dir) {
        return dir->getType() == ast::DirectiveType::printsize ||
               dir->getType() == ast::DirectiveType::output;
    });
}

std::vector<ast::Directive*> TranslatorContext::getLoadDirectives(const ast::QualifiedName& name) const {
    return filter(program->getDirectives(name),
            [&](const ast::Directive* dir) { return dir->getType() == ast::DirectiveType::input; });
}

bool TranslatorContext::hasSizeLimit(const ast::Relation* relation) const {
    return ioType->isLimitSize(relation);
}

std::size_t TranslatorContext::getSizeLimit(const ast::Relation* relation) const {
    assert(hasSizeLimit(relation) && "relation does not have a size limit");
    return ioType->getLimitSize(relation);
}

std::set<const ast::Relation*> TranslatorContext::getRelationsInSCC(std::size_t scc) const {
    return sccGraph->getInternalRelations(scc);
}

std::set<const ast::Relation*> TranslatorContext::getInputRelationsInSCC(std::size_t scc) const {
    return sccGraph->getInternalInputRelations(scc);
}

std::set<const ast::Relation*> TranslatorContext::getOutputRelationsInSCC(std::size_t scc) const {
    return sccGraph->getInternalOutputRelations(scc);
}

VecOwn<ram::Statement> TranslatorContext::getRecursiveUniqueKeyStatementsInSCC(std::size_t scc) const {
    VecOwn<ram::Statement> res;
    for (auto&& s : uniqueKeysAnalysis->getUniqueKeyStatementsInSCC(scc)) {
        if (s->isRecursiveRelation()) {
            res.push_back(clone(s));
        }
    }
    return res;
}

VecOwn<ram::Statement> TranslatorContext::getNonRecursiveUniqueKeyStatementsInSCC(std::size_t scc) const {
    VecOwn<ram::Statement> res;
    for (auto&& s : uniqueKeysAnalysis->getUniqueKeyStatementsInSCC(scc)) {
        if (!s->isRecursiveRelation()) {
            res.push_back(clone(s));
        }
    }
    return res;
}

std::set<const ast::Relation*> TranslatorContext::getExpiredRelations(std::size_t scc) const {
    return relationSchedule->schedule().at(scc).expired();
}

bool TranslatorContext::hasSubsumptiveClause(const ast::QualifiedName& name) const {
    for (const auto* clause : getProgram()->getClauses(name)) {
        if (isA<ast::SubsumptiveClause>(clause)) {
            return true;
        }
    }
    return false;
}

TypeAttribute TranslatorContext::getFunctorReturnTypeAttribute(const ast::Functor& functor) const {
    return typeAnalysis->getFunctorReturnTypeAttribute(functor);
}

TypeAttribute TranslatorContext::getFunctorParamTypeAtribute(
        const ast::Functor& functor, std::size_t idx) const {
    return typeAnalysis->getFunctorParamTypeAttribute(functor, idx);
}

std::vector<TypeAttribute> TranslatorContext::getFunctorParamTypeAtributes(
        const ast::UserDefinedFunctor& udf) const {
    return typeAnalysis->getFunctorParamTypeAttributes(udf);
}

bool TranslatorContext::isStatefulFunctor(const ast::UserDefinedFunctor& udf) const {
    return functorAnalysis->isStatefulFunctor(udf);
}

ast::NumericConstant::Type TranslatorContext::getInferredNumericConstantType(
        const ast::NumericConstant& nc) const {
    return polyAnalysis->getInferredType(nc);
}

AggregateOp TranslatorContext::getOverloadedAggregatorOperator(const ast::Aggregator& aggr) const {
    return polyAnalysis->getOverloadedOperator(aggr);
}

BinaryConstraintOp TranslatorContext::getOverloadedBinaryConstraintOperator(
        const ast::BinaryConstraint& bc) const {
    return polyAnalysis->getOverloadedOperator(bc);
}

FunctorOp TranslatorContext::getOverloadedFunctorOp(const ast::IntrinsicFunctor& inf) const {
    return polyAnalysis->getOverloadedFunctionOp(inf);
}

bool TranslatorContext::isADTEnum(const ast::BranchInit* adt) const {
    return ast::analysis::isADTEnum(sumTypeBranches->unsafeGetType(adt->getBranchName()));
}

int TranslatorContext::getADTBranchId(const ast::BranchInit* adt) const {
    const auto& type = sumTypeBranches->unsafeGetType(adt->getBranchName());
    const auto& branches = type.getBranches();
    ast::analysis::AlgebraicDataType::Branch searchDummy{adt->getBranchName(), {}};
    auto iterToBranch = std::lower_bound(branches.begin(), branches.end(), searchDummy,
            [](const ast::analysis::AlgebraicDataType::Branch& left,
                    const ast::analysis::AlgebraicDataType::Branch& right) {
                return left.name < right.name;
            });
    return static_cast<int>(std::distance(std::begin(branches), iterToBranch));
}

bool TranslatorContext::isADTBranchSimple(const ast::BranchInit* adt) const {
    std::size_t arity = adt->getArguments().size();
    return arity <= 1;
}

Own<ram::Statement> TranslatorContext::translateNonRecursiveClause(
        const ast::Clause& clause, TranslationMode mode) const {
    auto clauseTranslator = Own<ClauseTranslator>(translationStrategy->createClauseTranslator(*this, mode));
    return clauseTranslator->translateNonRecursiveClause(clause);
}

Own<ram::Statement> TranslatorContext::translateRecursiveClause(const ast::Clause& clause,
        const std::set<const ast::Relation*>& scc, std::size_t version, TranslationMode mode) const {
    auto clauseTranslator = Own<ClauseTranslator>(translationStrategy->createClauseTranslator(*this, mode));
    return clauseTranslator->translateRecursiveClause(clause, scc, version);
}

Own<ram::Expression> TranslatorContext::translateValue(
        const ValueIndex& index, const ast::Argument* arg) const {
    auto valueTranslator = Own<ValueTranslator>(translationStrategy->createValueTranslator(*this, index));
    return valueTranslator->translateValue(arg);
}

Own<ram::Condition> TranslatorContext::translateConstraint(
        const ValueIndex& index, const ast::Literal* lit) const {
    auto constraintTranslator =
            Own<ConstraintTranslator>(translationStrategy->createConstraintTranslator(*this, index));
    return constraintTranslator->translateConstraint(lit);
}

}  // namespace souffle::ast2ram
