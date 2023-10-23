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

#include "SumTypeBranches.h"
#include "Type.h"
#include "TypeEnvironment.h"
#include "TypeSystem.h"
#include "ast/analysis/Constraint.h"
#include "ast/analysis/ConstraintSystem.h"
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

template <>
struct detail::default_is_valid_op<TypeSet> {
    bool operator()(const TypeSet& a) {
        return !a.empty();
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

TypeConstraint isSubtypeOf(const TypeVar& a, const TypeVar& b);
TypeConstraint isSubtypeOf(const TypeVar& variable, const Type& type);
TypeConstraint hasSuperTypeInSet(const TypeVar& var, TypeSet values);
TypeConstraint subtypesOfTheSameBaseType(const TypeVar& left, const TypeVar& right);
TypeConstraint satisfiesOverload(const TypeEnvironment& typeEnv, IntrinsicFunctors overloads, TypeVar result,
        std::vector<TypeVar> args, bool subtypeResult);
TypeConstraint isSubtypeOfComponent(
        const TypeVar& elementVariable, const TypeVar& recordVariable, std::size_t index);

class TypeAnalysis::TypeErrorAnalyzer : public ErrorAnalyzer<TypeVar> {};

}  // namespace souffle::ast::analysis
