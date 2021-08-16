/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file PolymorphicObjects.h
 *
 ***********************************************************************/

#pragma once

#include "ast/NumericConstant.h"
#include "ast/TranslationUnit.h"
#include <map>
#include <set>

namespace souffle {
enum class AggregateOp;
enum class BinaryConstraintOp;
enum class FunctorOp;
}  // namespace souffle

namespace souffle::ast {
class Aggregator;
class BinaryConstraint;
class IntrinsicFunctor;
class TranslationUnit;
}  // namespace souffle::ast

namespace souffle::ast::analysis {

class TypeAnalysis;

class PolymorphicObjectsAnalysis : public Analysis {
public:
    static constexpr const char* name = "polymorphic-objects";

    PolymorphicObjectsAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    // Numeric constants
    bool hasInvalidType(const NumericConstant& nc) const;
    NumericConstant::Type getInferredType(const NumericConstant& nc) const;

    // Functors
    FunctorOp getOverloadedFunctionOp(const IntrinsicFunctor& inf) const;

    // Binary constraints
    BinaryConstraintOp getOverloadedOperator(const BinaryConstraint& bc) const;

    // Aggregators
    AggregateOp getOverloadedOperator(const Aggregator& agg) const;

private:
    const TypeAnalysis* typeAnalysis = nullptr;
};

}  // namespace souffle::ast::analysis
