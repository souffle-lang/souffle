/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeConstraintsAnalysis.h
 *
 ***********************************************************************/

#pragma once

#include "ast/analysis/typesystem/TypeConstraints.h"

namespace souffle::ast::analysis {

/**
 * Constraint analysis framework for types.
 *
 * The analysis operates on the concept of sinks and sources.
 * If the atom is negated or is a head then it's a sink,
 * and we can only extract the kind constraint from it
 * Otherwise it is a source, and the type of the element must
 * be a subtype of source attribute.
 */
class TypeConstraintsAnalysis : public ConstraintAnalysis<TypeVar> {
public:
    TypeConstraintsAnalysis(const TranslationUnit& tu) : tu(tu) {}

private:
    const TranslationUnit& tu;
    const TypeEnvironment& typeEnv = tu.getAnalysis<TypeEnvironmentAnalysis>().getTypeEnvironment();
    const Program& program = tu.getProgram();
    const SumTypeBranchesAnalysis& sumTypesBranches = tu.getAnalysis<SumTypeBranchesAnalysis>();
    const TypeAnalysis& typeAnalysis = tu.getAnalysis<TypeAnalysis>();
    const FunctorAnalysis& functorAnalysis = tu.getAnalysis<FunctorAnalysis>();

    // Sinks = {head} ∪ {negated atoms}
    std::set<const Atom*> sinks;

    /**
     * Utility function.
     * Iterate over atoms valid pairs of (argument, type-attribute) and apply procedure `map` for its
     * side-effects.
     */
    void iterateOverAtom(const Atom& atom, std::function<void(const Argument&, const Type&)> map);

    /** Visitors */
    void collectConstraints(const Clause& clause) override;
    void visitSink(const Atom& atom);
    void visit_(type_identity<Atom>, const Atom& atom) override;
    void visit_(type_identity<Negation>, const Negation& cur) override;
    void visit_(type_identity<StringConstant>, const StringConstant& cnst) override;
    void visit_(type_identity<NumericConstant>, const NumericConstant& constant) override;
    void visit_(type_identity<BinaryConstraint>, const BinaryConstraint& rel) override;
    void visit_(type_identity<IntrinsicFunctor>, const IntrinsicFunctor& fun) override;
    void visit_(type_identity<UserDefinedFunctor>, const UserDefinedFunctor& fun) override;
    void visit_(type_identity<Counter>, const Counter& counter) override;
    void visit_(type_identity<IterationCounter>, const IterationCounter& counter) override;
    void visit_(type_identity<TypeCast>, const ast::TypeCast& typeCast) override;
    void visit_(type_identity<RecordInit>, const RecordInit& record) override;
    void visit_(type_identity<BranchInit>, const BranchInit& adt) override;
    void visit_(type_identity<IntrinsicAggregator>, const IntrinsicAggregator& agg) override;
    void visit_(type_identity<UserDefinedAggregator>, const UserDefinedAggregator& agg) override;
};

}  // namespace souffle::ast::analysis