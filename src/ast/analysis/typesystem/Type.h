/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Type.h
 *
 * A collection of type analyses operating on AST constructs.
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "FunctorOps.h"
#include "ast/Clause.h"
#include "ast/NumericConstant.h"
#include "ast/TranslationUnit.h"
#include "ast/UserDefinedAggregator.h"
#include "ast/analysis/Functor.h"
#include "ast/analysis/typesystem/SumTypeBranches.h"
#include "ast/analysis/typesystem/TypeEnvironment.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "souffle/BinaryConstraintOps.h"
#include <cstddef>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace souffle::ast {
class Argument;
class Aggregator;
class IntrinsicAggregator;
class BinaryConstraint;
class Clause;
class Functor;
class FunctorDeclaration;
class IntrinsicFunctor;
class IterationCounter;
class NumericConstant;
class Type;
class UserDefinedFunctor;
class Negation;
class StringConstant;
class NilConstant;
class Counter;
class TypeCast;
class BranchInit;
class RecordInit;
}  // namespace souffle::ast

namespace souffle::ast::analysis {
class TypeEnvironment;

class TypeAnalysis : public Analysis {
public:
    class TypeErrorAnalyzer;

    static constexpr const char* name = "type-analysis";

    TypeAnalysis();
    ~TypeAnalysis();

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    /** Get the computed types for the given argument. */
    TypeSet const& getTypes(const Argument* argument) const {
        return argumentTypes.at(argument);
    }

    /**
     * Analyse the given clause and computes for each contained argument
     * a set of potential types. If the set associated to an argument is empty,
     * no consistent typing can be found and the rule can not be properly typed.
     *
     * @return a map mapping each contained argument to a set of types
     */
    static std::map<const Argument*, TypeSet> analyseTypes(const TranslationUnit& tu, const Clause& clause,
            TypeErrorAnalyzer* errorAnalyzer = nullptr, std::ostream* logs = nullptr);

    // Checks whether an argument has been assigned a valid type
    bool hasValidTypeInfo(const Argument& argument) const;
    /** Check whether a functor declaration has valid type info */
    bool hasValidTypeInfo(const FunctorDeclaration& decl) const;

    std::set<TypeAttribute> getTypeAttributes(const Argument* arg) const;

    /** -- Functor-related methods -- */
    IntrinsicFunctors getValidIntrinsicFunctorOverloads(const IntrinsicFunctor& inf) const;
    TypeAttribute getFunctorReturnTypeAttribute(const Functor& functor) const;
    Type const& getFunctorReturnType(const UserDefinedFunctor& functor) const;
    Type const& getFunctorParamType(const UserDefinedFunctor& functor, std::size_t idx) const;
    TypeAttribute getFunctorParamTypeAttribute(const Functor& functor, std::size_t idx) const;
    std::vector<TypeAttribute> getFunctorParamTypeAttributes(const UserDefinedFunctor& functor) const;

    /** -- User-defined Aggregator-related methods -- */
    TypeAttribute getAggregatorReturnTypeAttribute(const UserDefinedAggregator& aggregator) const;
    Type const& getAggregatorReturnType(const UserDefinedAggregator& aggregator) const;
    Type const& getAggregatorParamType(const UserDefinedAggregator& aggregator, std::size_t idx) const;
    TypeAttribute getAggregatorParamTypeAttribute(
            const UserDefinedAggregator& aggregator, std::size_t idx) const;
    std::vector<TypeAttribute> getAggregatorParamTypeAttributes(
            const UserDefinedAggregator& aggregator) const;

    /** -- Polymorphism-related methods -- */
    NumericConstant::Type getPolymorphicNumericConstantType(const NumericConstant& nc) const;
    const std::map<const NumericConstant*, NumericConstant::Type>& getNumericConstantTypes() const;
    AggregateOp getPolymorphicOperator(const IntrinsicAggregator& agg) const;
    BinaryConstraintOp getPolymorphicOperator(const BinaryConstraint& bc) const;
    FunctorOp getPolymorphicOperator(const IntrinsicFunctor& inf) const;

private:
    // General type analysis
    TypeEnvironment const* typeEnv = nullptr;
    FunctorAnalysis const* functorAnalysis = nullptr;
    std::map<const Argument*, TypeSet> argumentTypes;

public:
    std::shared_ptr<TypeErrorAnalyzer> errorAnalyzer;

private:
    VecOwn<Clause> annotatedClauses;
    std::stringstream analysisLogs;
    const TranslationUnit* translationUnit;

    /* Return a new clause with type-annotated variables */
    static Own<Clause> createAnnotatedClause(
            const Clause* clause, const std::map<const Argument*, TypeSet> argumentTypes);

    // Polymorphic objects analysis
    std::map<const IntrinsicFunctor*, const IntrinsicFunctorInfo*> functorInfo;
    std::map<const NumericConstant*, NumericConstant::Type> numericConstantType;
    std::map<const IntrinsicAggregator*, AggregateOp> aggregatorType;
    std::map<const BinaryConstraint*, BinaryConstraintOp> constraintType;

    bool analyseIntrinsicFunctors(const TranslationUnit& translationUnit);
    bool analyseNumericConstants(const TranslationUnit& translationUnit);
    bool analyseAggregators(const TranslationUnit& translationUnit);
    bool analyseBinaryConstraints(const TranslationUnit& translationUnit);

    bool isFloat(const Argument* argument) const;
    bool isUnsigned(const Argument* argument) const;
    bool isSymbol(const Argument* argument) const;

    /** Convert a qualified name to its type */
    Type const& nameToType(QualifiedName const& name) const;

    /** Convert a qualified name to a TypeAttribute */
    TypeAttribute nameToTypeAttribute(QualifiedName const& name) const;
};

/**
 * Printer for Type Annotationed Clauses
 */
class TypeAnnotationPrinter {
public:
    TypeAnnotationPrinter(const TranslationUnit* tu, const std::map<const Argument*, TypeSet> argumentTypes,
            std::ostream& os)
            : tu(tu), argumentTypes(argumentTypes), os(os) {}

    void printAnnotatedClause(const Clause& clause);

private:
    const TranslationUnit* tu;
    const TypeEnvironmentAnalysis& tea = tu->getAnalysis<TypeEnvironmentAnalysis>();
    const TypeEnvironment& typeEnv = tea.getTypeEnvironment();
    const Program& program = tu->getProgram();
    const SumTypeBranchesAnalysis& sumTypesBranches = tu->getAnalysis<SumTypeBranchesAnalysis>();
    const TypeAnalysis& typeAnalysis = tu->getAnalysis<TypeAnalysis>();

    std::map<const Argument*, TypeSet> argumentTypes;
    std::ostream& os;

    void branchOnArgument(const Argument*, const Type&);
    void print_(type_identity<Atom>, const Atom& atom);
    void print_(type_identity<Negation>, const Negation& cur);
    void print_(type_identity<StringConstant>, const StringConstant& cnst);
    void print_(type_identity<NumericConstant>, const NumericConstant& constant);
    void print_(type_identity<NilConstant>, const NilConstant& constant);
    void print_(type_identity<BinaryConstraint>, const BinaryConstraint& rel);
    void print_(type_identity<IntrinsicFunctor>, const IntrinsicFunctor& fun);
    void print_(type_identity<UserDefinedFunctor>, const UserDefinedFunctor& fun);
    void print_(type_identity<Counter>, const Counter& counter);
    void print_(type_identity<IterationCounter>, const IterationCounter& counter);
    void print_(type_identity<TypeCast>, const ast::TypeCast& typeCast);
    void print_(type_identity<RecordInit>, const RecordInit& record, const RecordType&);
    void print_(type_identity<BranchInit>, const BranchInit& adt);
    void print_(type_identity<Aggregator>, const Aggregator& agg);
    void printBodyLiterals(std::vector<Literal*> literals, const std::string& spc);
};

}  // namespace souffle::ast::analysis
