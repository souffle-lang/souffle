/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeConstraints.cpp
 *
 ***********************************************************************/

#include "ast/analysis/typesystem/TypeConstraints.h"
#include "ast/analysis/typesystem/Type.h"

namespace souffle::ast::analysis {

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of the variable b.
 */
TypeConstraint isSubtypeOf(const TypeVar& a, const TypeVar& b) {
    return sub(a, b, "<:");
}

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of type b.
 */
TypeConstraint isSubtypeOf(const TypeVar& variable, const Type& type) {
    struct C : public Constraint<TypeVar> {
        TypeVar variable;
        const Type& type;

        C(TypeVar variable, const Type& type) : variable(std::move(variable)), type(type) {}

        bool update(Assignment<TypeVar>& assignments) const override {
            TypeSet& assignment = assignments[variable];

            if (assignment.isAll()) {
                assignment = TypeSet(skipAliasesType(type));
                return true;
            }

            TypeSet newAssignment;
            for (const Type& t : assignment) {
                assert(!isA<AliasType>(t));
                newAssignment.insert(getGreatestCommonSubtypes(t, type));
            }

            // check whether there was a change
            if (assignment == newAssignment) {
                return false;
            }
            assignment = newAssignment;
            return true;
        }

        void print(std::ostream& out) const override {
            out << variable << " <: " << type.getName();
        }
    };

    return std::make_shared<C>(variable, type);
}

/**
 * A constraint factory ensuring that all the types associated to the variable
 * are subtypes of some type in the provided set (values)
 *
 * Values can't be all.
 */
TypeConstraint hasSuperTypeInSet(const TypeVar& var, TypeSet values) {
    struct C : public Constraint<TypeVar> {
        TypeVar var;
        TypeSet values;

        C(TypeVar var, TypeSet values) : var(std::move(var)), values(std::move(values)) {}

        bool update(Assignment<TypeVar>& assignment) const override {
            // get current value of variable a
            TypeSet& assignments = assignment[var];

            // remove all types that are not sub-types of b
            if (assignments.isAll()) {
                assignments = values;
                return true;
            }

            TypeSet newAssignments;
            for (const Type& type : assignments) {
                assert(!isA<AliasType>(type));
                bool existsSuperTypeInValues =
                        any_of(values, [&type](const Type& value) { return isSubtypeOf(type, value); });
                if (existsSuperTypeInValues) {
                    newAssignments.insert(type);
                }
            }
            // check whether there was a change
            if (newAssignments == assignments) {
                return false;
            }
            assignments = newAssignments;
            return true;
        }

        void print(std::ostream& out) const override {
            out << "∃ t ∈ " << values << ": " << var << " <: t";
        }
    };

    return std::make_shared<C>(var, values);
}

/**
 * Ensure that types of left and right have the same base types.
 */
TypeConstraint subtypesOfTheSameBaseType(const TypeVar& left, const TypeVar& right) {
    struct C : public Constraint<TypeVar> {
        TypeVar left;
        TypeVar right;

        C(TypeVar left, TypeVar right) : left(std::move(left)), right(std::move(right)) {}

        bool update(Assignment<TypeVar>& assignment) const override {
            // get current value of variable a
            TypeSet& assignmentsLeft = assignment[left];
            TypeSet& assignmentsRight = assignment[right];

            // Base types common to left and right variables.
            TypeSet baseTypes;

            // Base types present in left/right variable.
            TypeSet baseTypesLeft;
            TypeSet baseTypesRight;

            // Iterate over possible types extracting base types.
            // Left
            if (!assignmentsLeft.isAll()) {
                for (const Type& type : assignmentsLeft) {
                    assert(!isA<AliasType>(type));
                    if (isA<SubsetType>(type) || isA<ConstantType>(type)) {
                        baseTypesLeft.insert(getBaseType(&type));
                    }
                }
            }
            // Right
            if (!assignmentsRight.isAll()) {
                for (const Type& type : assignmentsRight) {
                    assert(!isA<AliasType>(type));
                    if (isA<SubsetType>(type) || isA<ConstantType>(type)) {
                        baseTypesRight.insert(getBaseType(&type));
                    }
                }
            }

            TypeSet resultLeft;
            TypeSet resultRight;

            // Handle all
            if (assignmentsLeft.isAll() && assignmentsRight.isAll()) {
                return false;
            }

            // If left xor right is all, assign base types of the other side as possible values.
            if (assignmentsLeft.isAll()) {
                assignmentsLeft = baseTypesRight;
                return true;
            }
            if (assignmentsRight.isAll()) {
                assignmentsRight = baseTypesLeft;
                return true;
            }

            baseTypes = TypeSet::intersection(baseTypesLeft, baseTypesRight);

            // Allow types if they are subtypes of any of the common base types.
            for (const Type& type : assignmentsLeft) {
                assert(!isA<AliasType>(type));
                bool isSubtypeOfCommonBaseType = any_of(baseTypes.begin(), baseTypes.end(),
                        [&type](const Type& baseType) { return isSubtypeOf(type, baseType); });
                if (isSubtypeOfCommonBaseType) {
                    resultLeft.insert(type);
                }
            }

            for (const Type& type : assignmentsRight) {
                assert(!isA<AliasType>(type));
                bool isSubtypeOfCommonBaseType = any_of(baseTypes.begin(), baseTypes.end(),
                        [&type](const Type& baseType) { return isSubtypeOf(type, baseType); });
                if (isSubtypeOfCommonBaseType) {
                    resultRight.insert(type);
                }
            }

            // check whether there was a change
            if (resultLeft == assignmentsLeft && resultRight == assignmentsRight) {
                return false;
            }
            assignmentsLeft = resultLeft;
            assignmentsRight = resultRight;
            return true;
        }
        //
        void print(std::ostream& out) const override {
            out << "∃ t : (" << left << " <: t)"
                << " ∧ "
                << "(" << right << " <: t)"
                << " where t is a base type";
        }
    };

    return std::make_shared<C>(left, right);
}

/**
 * Given a set of overloads, wait the list of candidates to reduce to one and then apply its constraints.
 * NOTE:  `subtypeResult` implies that `func <: overload-return-type`, rather than
 *        `func = overload-return-type`. This is required for old type semantics.
 *        See #1296 and tests/semantic/type_system4
 */
TypeConstraint satisfiesOverload(const TypeEnvironment& typeEnv, IntrinsicFunctors overloads, TypeVar result,
        std::vector<TypeVar> args, bool subtypeResult) {
    struct C : public Constraint<TypeVar> {
        // Check if there already was a non-monotonic update
        mutable bool nonMonotonicUpdate = false;

        const TypeEnvironment& typeEnv;
        mutable IntrinsicFunctors overloads;
        TypeVar result;
        std::vector<TypeVar> args;
        bool subtypeResult;

        C(const TypeEnvironment& typeEnv, IntrinsicFunctors overloads, TypeVar result,
                std::vector<TypeVar> args, bool subtypeResult)
                : typeEnv(typeEnv), overloads(std::move(overloads)), result(std::move(result)),
                  args(std::move(args)), subtypeResult(subtypeResult) {}

        bool update(Assignment<TypeVar>& assignment) const override {
            auto subtypesOf = [&](const TypeSet& src, TypeAttribute tyAttr) {
                auto& ty = typeEnv.getConstantType(tyAttr);
                return src.filter(TypeSet(true), [&](auto&& x) { return isSubtypeOf(x, ty); });
            };

            auto possible = [&](TypeAttribute ty, const TypeVar& var) {
                auto& curr = assignment[var];
                return curr.isAll() || any_of(curr, [&](auto&& t) { return getTypeAttribute(t) == ty; });
            };

            overloads = filterNot(std::move(overloads), [&](const IntrinsicFunctorInfo& x) -> bool {
                if (!x.variadic && args.size() != x.params.size()) return true;  // arity mismatch?

                for (std::size_t i = 0; i < args.size(); ++i) {
                    if (!possible(x.params[x.variadic ? 0 : i], args[i])) return true;
                }

                return !possible(x.result, result);
            });

            bool changed = false;
            auto newResult = [&]() -> std::optional<TypeSet> {
                if (0 == overloads.size()) return TypeSet();
                if (1 < overloads.size()) return {};

                auto& overload = overloads.front().get();
                // `ord` is freakin' magical: it has the signature `a -> Int`.
                // As a consequence, we might be given non-primitive arguments (i.e. types for which
                // `TypeEnv::getConstantType` is undefined).
                // Handle this by not imposing constraints on the arguments.
                if (overload.op != FunctorOp::ORD) {
                    for (std::size_t i = 0; i < args.size(); ++i) {
                        auto argTy = overload.params[overload.variadic ? 0 : i];
                        auto& currArg = assignment[args[i]];
                        auto newArg = subtypesOf(currArg, argTy);
                        changed |= currArg != newArg;
                        // 2020-05-09: CI linter says to remove `std::move`, but clang-tidy-10 is happy.
                        currArg = std::move(newArg);  // NOLINT
                    }
                }

                if (nonMonotonicUpdate || subtypeResult) {
                    return subtypesOf(assignment[result], overload.result);
                } else {
                    nonMonotonicUpdate = true;
                    return TypeSet{typeEnv.getConstantType(overload.result)};
                }
            }();

            if (newResult) {
                auto& curr = assignment[result];
                changed |= curr != *newResult;
                // 2020-05-09: CI linter says to remove `std::move`, but clang-tidy-10 is happy.
                curr = std::move(*newResult);  // NOLINT
            }

            return changed;
        }

        void print(std::ostream& out) const override {
            // TODO (darth_tytus): is this description correct?
            out << "∃ t : " << result << " <: t where t is a base type";
        }
    };

    return std::make_shared<C>(
            typeEnv, std::move(overloads), std::move(result), std::move(args), subtypeResult);
}

/**
 * Constraint on record type and its elements.
 */
TypeConstraint isSubtypeOfComponent(
        const TypeVar& elementVariable, const TypeVar& recordVariable, std::size_t index) {
    struct C : public Constraint<TypeVar> {
        TypeVar elementVariable;
        TypeVar recordVariable;
        std::size_t index;

        C(TypeVar elementVariable, TypeVar recordVariable, std::size_t index)
                : elementVariable(std::move(elementVariable)), recordVariable(std::move(recordVariable)),
                  index(index) {}

        bool update(Assignment<TypeVar>& assignment) const override {
            // get list of types for b
            const TypeSet& recordTypes = assignment[recordVariable];

            // if it is (not yet) constrainted => skip
            if (recordTypes.isAll()) {
                return false;
            }

            // compute new types for element and record
            TypeSet newElementTypes;
            TypeSet newRecordTypes;

            for (const Type& type : recordTypes) {
                assert(!isA<AliasType>(type));
                // A type must be either a record type or a subset of a record type
                if (!isBaseOfKind(type, TypeAttribute::Record)) {
                    continue;
                }

                const auto& typeAsRecord = *as<RecordType>(getBaseType(&type));

                // Wrong size => skip.
                if (typeAsRecord.getFields().size() <= index) {
                    continue;
                }

                // Valid type for record.
                newRecordTypes.insert(type);

                // and its corresponding field.
                newElementTypes.insert(skipAliasesType(*typeAsRecord.getFields()[index]));
            }

            // combine with current types assigned to element
            newElementTypes = getGreatestCommonSubtypes(assignment[elementVariable], newElementTypes);

            // update values
            bool changed = false;
            if (newRecordTypes != recordTypes) {
                assignment[recordVariable] = newRecordTypes;
                changed = true;
            }

            if (assignment[elementVariable] != newElementTypes) {
                assignment[elementVariable] = newElementTypes;
                changed = true;
            }

            return changed;
        }

        void print(std::ostream& out) const override {
            out << elementVariable << " <: " << recordVariable << "::" << index;
        }
    };

    return std::make_shared<C>(elementVariable, recordVariable, index);
}

}  // namespace souffle::ast::analysis
