/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeConstraints.h
 *
 ***********************************************************************/

#pragma once

#include "ast/analysis/Constraint.h"
#include "ast/analysis/ConstraintSystem.h"
#include "ast/analysis/SumTypeBranches.h"
#include "ast/analysis/TypeEnvironment.h"
#include "ast/analysis/TypeSystem.h"
#include "ast/utility/Utils.h"
#include "souffle/utility/StringUtil.h"

namespace souffle::ast::analysis {

// -----------------------------------------------------------------------------
//                          Type Deduction Lattice
// -----------------------------------------------------------------------------

/**
 * An implementation of a meet operation between sets of types computing
 * the set of pair-wise greatest common subtypes.
 */
struct sub_type {
    bool operator()(TypeSet& a, const TypeSet& b) const {
        // compute result set
        TypeSet greatestCommonSubtypes = getGreatestCommonSubtypes(a, b);

        // check whether a should change
        if (greatestCommonSubtypes == a) {
            return false;
        }

        // update a
        a = greatestCommonSubtypes;
        return true;
    }
};

/**
 * A factory for computing sets of types covering all potential types.
 */
struct all_type_factory {
    TypeSet operator()() const {
        return TypeSet(true);
    }
};

/**
 * The type lattice forming the property space for the Type analysis. The
 * value set is given by sets of types and the meet operator is based on the
 * pair-wise computation of greatest common subtypes. Correspondingly, the
 * bottom element has to be the set of all types.
 */
struct type_lattice : public property_space<TypeSet, sub_type, all_type_factory> {};

/** The definition of the type of variable to be utilized in the type analysis */
using TypeVar = ConstraintAnalysisVar<type_lattice>;

/** The definition of the type of constraint to be utilized in the type analysis */
using TypeConstraint = std::shared_ptr<Constraint<TypeVar>>;

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
    const TypeEnvironment& typeEnv = tu.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();
    const Program& program = tu.getProgram();
    const SumTypeBranchesAnalysis& sumTypesBranches = *tu.getAnalysis<SumTypeBranchesAnalysis>();
    const TypeAnalysis& typeAnalysis = *tu.getAnalysis<TypeAnalysis>();

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
    void visit_(type_identity<TypeCast>, const ast::TypeCast& typeCast) override;
    [[maybe_unused]] void visit_(type_identity<RecordInit>, const RecordInit& record) override;
    void visit_(type_identity<BranchInit>, const BranchInit& adt) override;
    void visit_(type_identity<Aggregator>, const Aggregator& agg) override;

    /** Recursively check objects of RecordType (which might be a recursive type) */
    void addConstraintsRecursively(const Argument&, const Type&);

    /** Add constraints related to exposed records used in binary constraints */
    void exposedRecord(const RecordInit&);
};

}  // namespace souffle::ast::analysis
