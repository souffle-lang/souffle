/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslatorContext.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "FunctorOps.h"
#include "ast/NumericConstant.h"
#include "ast/QualifiedName.h"
#include "ast/analysis/ProfileUse.h"
#include "ast/analysis/UniqueKeys.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast2ram/ClauseTranslator.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include <cstddef>
#include <set>
#include <vector>

namespace souffle::ast {
class Aggregator;
class Atom;
class BinaryConstraint;
class BranchInit;
class Clause;
class Directive;
class Functor;
class IntrinsicFunctor;
class Literal;
class Program;
class QualifiedName;
class Relation;
class SipsMetric;
class TranslationUnit;
class UserDefinedFunctor;
}  // namespace souffle::ast

namespace souffle::ram {
class Condition;
class Expression;
class Statement;
}  // namespace souffle::ram

namespace souffle::ast::analysis {
class FunctorAnalysis;
class IOTypeAnalysis;
class PolymorphicObjectsAnalysis;
class ProfileUseAnalysis;
class RecursiveClausesAnalysis;
class RelationScheduleAnalysis;
class SumTypeBranchesAnalysis;
class SCCGraphAnalysis;
class TypeEnvironment;
class UniqueKeysAnalysis;
}  // namespace souffle::ast::analysis

namespace souffle::ast2ram {

class TranslationStrategy;
class ValueIndex;

class TranslatorContext {
public:
    TranslatorContext(const ast::TranslationUnit& tu);
    ~TranslatorContext();

    const ast::Program* getProgram() const {
        return program;
    }

    /** Relation methods */
    std::vector<ast::Directive*> getStoreDirectives(const ast::QualifiedName& name) const;
    std::vector<ast::Directive*> getLoadDirectives(const ast::QualifiedName& name) const;
    std::string getAttributeTypeQualifier(const ast::QualifiedName& name) const;
    bool hasSizeLimit(const ast::Relation* relation) const;
    std::size_t getSizeLimit(const ast::Relation* relation) const;

    /** Clause methods */
    bool hasSubsumptiveClause(const ast::QualifiedName& name) const;
    bool isRecursiveClause(const ast::Clause* clause) const;
    std::size_t getClauseNum(const ast::Clause* clause) const;

    /** SCC methods */
    std::size_t getNumberOfSCCs() const;
    bool isRecursiveSCC(std::size_t scc) const;
    std::set<const ast::Relation*> getExpiredRelations(std::size_t scc) const;
    std::set<const ast::Relation*> getRelationsInSCC(std::size_t scc) const;
    std::set<const ast::Relation*> getInputRelationsInSCC(std::size_t scc) const;
    std::set<const ast::Relation*> getOutputRelationsInSCC(std::size_t scc) const;

    /** UniqueKeys methods */
    VecOwn<ram::Statement> getRecursiveUniqueKeyStatementsInSCC(std::size_t scc) const;
    VecOwn<ram::Statement> getNonRecursiveUniqueKeyStatementsInSCC(std::size_t scc) const;

    /** Functor methods */
    TypeAttribute getFunctorReturnTypeAttribute(const ast::Functor& functor) const;
    TypeAttribute getFunctorParamTypeAtribute(const ast::Functor& functor, std::size_t idx) const;
    std::vector<TypeAttribute> getFunctorParamTypeAtributes(const ast::UserDefinedFunctor& udf) const;
    bool isStatefulFunctor(const ast::UserDefinedFunctor& functor) const;

    /** ADT methods */
    bool isADTEnum(const ast::BranchInit* adt) const;
    int getADTBranchId(const ast::BranchInit* adt) const;
    bool isADTBranchSimple(const ast::BranchInit* adt) const;

    /** Polymorphic objects methods */
    ast::NumericConstant::Type getInferredNumericConstantType(const ast::NumericConstant& nc) const;
    AggregateOp getOverloadedAggregatorOperator(const ast::Aggregator& aggr) const;
    BinaryConstraintOp getOverloadedBinaryConstraintOperator(const ast::BinaryConstraint& bc) const;
    FunctorOp getOverloadedFunctorOp(const ast::IntrinsicFunctor& inf) const;

    /** Analyses */
    const ast::SipsMetric* getSipsMetric() const {
        return sipsMetric.get();
    }

    /** Translation strategy */
    Own<ram::Statement> translateNonRecursiveClause(
            const ast::Clause& clause, TranslationMode mode = DEFAULT) const;
    Own<ram::Statement> translateRecursiveClause(const ast::Clause& clause,
            const std::set<const ast::Relation*>& scc, std::size_t version,
            TranslationMode mode = DEFAULT) const;

    Own<ram::Condition> translateConstraint(const ValueIndex& index, const ast::Literal* lit) const;

    Own<ram::Expression> translateValue(const ValueIndex& index, const ast::Argument* arg) const;

private:
    const ast::Program* program;
    const ast::analysis::RecursiveClausesAnalysis* recursiveClauses;
    const ast::analysis::RelationScheduleAnalysis* relationSchedule;
    const ast::analysis::SCCGraphAnalysis* sccGraph;
    const ast::analysis::FunctorAnalysis* functorAnalysis;
    const ast::analysis::IOTypeAnalysis* ioType;
    const ast::analysis::TypeAnalysis* typeAnalysis;
    const ast::analysis::TypeEnvironment* typeEnv;
    const ast::analysis::SumTypeBranchesAnalysis* sumTypeBranches;
    const ast::analysis::PolymorphicObjectsAnalysis* polyAnalysis;
    const ast::analysis::UniqueKeysAnalysis* uniqueKeysAnalysis;
    std::map<const ast::Clause*, std::size_t> clauseNums;
    Own<ast::SipsMetric> sipsMetric;
    Own<TranslationStrategy> translationStrategy;
};

}  // namespace souffle::ast2ram
