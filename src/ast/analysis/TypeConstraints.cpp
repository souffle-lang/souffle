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

#include "ast/analysis/TypeConstraints.h"
#include "ast/analysis/Type.h"

#include <cassert>

namespace souffle::ast::analysis {

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of the variable b.
 */
static TypeConstraint isSubtypeOf(const TypeVar& a, const TypeVar& b) {
    return sub(a, b, "<:");
}

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of type b.
 */
static TypeConstraint isSubtypeOf(const TypeVar& variable, const Type& type) {
    struct C : public Constraint<TypeVar> {
        TypeVar variable;
        const Type& type;

        C(TypeVar variable, const Type& type) : variable(std::move(variable)), type(type) {}

        bool update(Assignment<TypeVar>& assignments) const override {
            TypeSet& assignment = assignments[variable];

            if (assignment.isAll()) {
                assignment = TypeSet(type);
                return true;
            }

            TypeSet newAssignment;
            for (const Type& t : assignment) {
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
static TypeConstraint hasSuperTypeInSet(const TypeVar& var, TypeSet values) {
    struct C : public Constraint<TypeVar> {
        TypeVar var;
        TypeSet values;

        C(TypeVar var, TypeSet values) : var(std::move(var)), values(std::move(values)) {}

        bool update(Assignment<TypeVar>& assigment) const override {
            // get current value of variable a
            TypeSet& assigments = assigment[var];

            // remove all types that are not sub-types of b
            if (assigments.isAll()) {
                assigments = values;
                return true;
            }

            TypeSet newAssigments;
            for (const Type& type : assigments) {
                bool existsSuperTypeInValues =
                        any_of(values, [&type](const Type& value) { return isSubtypeOf(type, value); });
                if (existsSuperTypeInValues) {
                    newAssigments.insert(type);
                }
            }
            // check whether there was a change
            if (newAssigments == assigments) {
                return false;
            }
            assigments = newAssigments;
            return true;
        }

        void print(std::ostream& out) const override {
            out << "∃ t ∈ " << values << ": " << var << " <: t";
        }
    };

    return std::make_shared<C>(var, values);
}

static const Type& getBaseType(const Type* type) {
    while (auto subset = as<SubsetType>(type)) {
        type = &subset->getBaseType();
    };
    assert((isA<ConstantType>(type) || isA<RecordType>(type)) &&
            "Root must be a constant type or a record type");
    return *type;
}

/**
 * Ensure that types of left and right have the same base types.
 */
static TypeConstraint subtypesOfTheSameBaseType(const TypeVar& left, const TypeVar& right) {
    struct C : public Constraint<TypeVar> {
        TypeVar left;
        TypeVar right;

        C(TypeVar left, TypeVar right) : left(std::move(left)), right(std::move(right)) {}

        bool update(Assignment<TypeVar>& assigment) const override {
            // get current value of variable a
            TypeSet& assigmentsLeft = assigment[left];
            TypeSet& assigmentsRight = assigment[right];

            // Base types common to left and right variables.
            TypeSet baseTypes;

            // Base types present in left/right variable.
            TypeSet baseTypesLeft;
            TypeSet baseTypesRight;

            // Iterate over possible types extracting base types.
            // Left
            if (!assigmentsLeft.isAll()) {
                for (const Type& type : assigmentsLeft) {
                    if (isA<SubsetType>(type) || isA<ConstantType>(type)) {
                        baseTypesLeft.insert(getBaseType(&type));
                    }
                }
            }
            // Right
            if (!assigmentsRight.isAll()) {
                for (const Type& type : assigmentsRight) {
                    if (isA<SubsetType>(type) || isA<ConstantType>(type)) {
                        baseTypesRight.insert(getBaseType(&type));
                    }
                }
            }

            TypeSet resultLeft;
            TypeSet resultRight;

            // Handle all
            if (assigmentsLeft.isAll() && assigmentsRight.isAll()) {
                return false;
            }

            // If left xor right is all, assign base types of the other side as possible values.
            if (assigmentsLeft.isAll()) {
                assigmentsLeft = baseTypesRight;
                return true;
            }
            if (assigmentsRight.isAll()) {
                assigmentsRight = baseTypesLeft;
                return true;
            }

            baseTypes = TypeSet::intersection(baseTypesLeft, baseTypesRight);

            // Allow types if they are subtypes of any of the common base types.
            for (const Type& type : assigmentsLeft) {
                bool isSubtypeOfCommonBaseType = any_of(baseTypes.begin(), baseTypes.end(),
                        [&type](const Type& baseType) { return isSubtypeOf(type, baseType); });
                if (isSubtypeOfCommonBaseType) {
                    resultLeft.insert(type);
                }
            }

            for (const Type& type : assigmentsRight) {
                bool isSubtypeOfCommonBaseType = any_of(baseTypes.begin(), baseTypes.end(),
                        [&type](const Type& baseType) { return isSubtypeOf(type, baseType); });
                if (isSubtypeOfCommonBaseType) {
                    resultRight.insert(type);
                }
            }

            // check whether there was a change
            if (resultLeft == assigmentsLeft && resultRight == assigmentsRight) {
                return false;
            }
            assigmentsLeft = resultLeft;
            assigmentsRight = resultRight;
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
static TypeConstraint satisfiesOverload(const TypeEnvironment& typeEnv, IntrinsicFunctors overloads,
        TypeVar result, std::vector<TypeVar> args, bool subtypeResult) {
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

        bool update(Assignment<TypeVar>& assigment) const override {
            auto subtypesOf = [&](const TypeSet& src, TypeAttribute tyAttr) {
                auto& ty = typeEnv.getConstantType(tyAttr);
                return src.filter(TypeSet(true), [&](auto&& x) { return isSubtypeOf(x, ty); });
            };

            auto possible = [&](TypeAttribute ty, const TypeVar& var) {
                auto& curr = assigment[var];
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
                        auto& currArg = assigment[args[i]];
                        auto newArg = subtypesOf(currArg, argTy);
                        changed |= currArg != newArg;
                        // 2020-05-09: CI linter says to remove `std::move`, but clang-tidy-10 is happy.
                        currArg = std::move(newArg);  // NOLINT
                    }
                }

                if (nonMonotonicUpdate || subtypeResult) {
                    return subtypesOf(assigment[result], overload.result);
                } else {
                    nonMonotonicUpdate = true;
                    return TypeSet{typeEnv.getConstantType(overload.result)};
                }
            }();

            if (newResult) {
                auto& curr = assigment[result];
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
static TypeConstraint isSubtypeOfComponent(
        const TypeVar& elementVariable, const TypeVar& recordVariable, std::size_t index) {
    struct C : public Constraint<TypeVar> {
        TypeVar elementVariable;
        TypeVar recordVariable;
        unsigned index;

        C(TypeVar elementVariable, TypeVar recordVariable, int index)
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
                // A type must be either a record type or a subset of a record type
                if (!isOfKind(type, TypeAttribute::Record)) {
                    continue;
                }

                const auto& typeAsRecord = *as<RecordType>(type);

                // Wrong size => skip.
                if (typeAsRecord.getFields().size() <= index) {
                    continue;
                }

                // Valid type for record.
                newRecordTypes.insert(type);

                // and its corresponding field.
                newElementTypes.insert(*typeAsRecord.getFields()[index]);
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

void TypeConstraintsAnalysis::visitSink(const Atom& atom) {
    iterateOverAtom(atom, [&](const Argument& argument, const Type& attributeType) {
        if (isA<RecordType>(attributeType)) {
            addConstraint(isSubtypeOf(getVar(argument), getBaseType(&attributeType)));
            return;
        }
        for (auto& constantType : typeEnv.getConstantTypes()) {
            if (isSubtypeOf(attributeType, constantType)) {
                addConstraint(isSubtypeOf(getVar(argument), constantType));
            }
        }
    });
}

void TypeConstraintsAnalysis::visit_(type_identity<Atom>, const Atom& atom) {
    if (contains(sinks, &atom)) {
        visitSink(atom);
        return;
    }

    iterateOverAtom(atom, [&](const Argument& argument, const Type& attributeType) {
        addConstraint(isSubtypeOf(getVar(argument), attributeType));
    });
}

void TypeConstraintsAnalysis::visit_(type_identity<Negation>, const Negation& cur) {
    sinks.insert(cur.getAtom());
}

void TypeConstraintsAnalysis::visit_(type_identity<StringConstant>, const StringConstant& cnst) {
    addConstraint(isSubtypeOf(getVar(cnst), typeEnv.getConstantType(TypeAttribute::Symbol)));
}

void TypeConstraintsAnalysis::visit_(type_identity<NumericConstant>, const NumericConstant& constant) {
    TypeSet possibleTypes;

    // Check if the type is given.
    if (constant.getFixedType().has_value()) {
        switch (constant.getFixedType().value()) {
            // Insert a type, but only after checking that parsing is possible.
            case NumericConstant::Type::Int:
                if (canBeParsedAsRamSigned(constant.getConstant())) {
                    possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
                }
                break;
            case NumericConstant::Type::Uint:
                if (canBeParsedAsRamUnsigned(constant.getConstant())) {
                    possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
                }
                break;
            case NumericConstant::Type::Float:
                if (canBeParsedAsRamFloat(constant.getConstant())) {
                    possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
                }
                break;
        }
    } else if (contains(typeAnalysis.getNumericConstantTypes(), &constant)) {
        switch (typeAnalysis.getNumericConstantTypes().at(&constant)) {
            // Insert a type, but only after checking that parsing is possible.
            case NumericConstant::Type::Int:
                if (canBeParsedAsRamSigned(constant.getConstant())) {
                    possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
                }
                break;
            case NumericConstant::Type::Uint:
                if (canBeParsedAsRamUnsigned(constant.getConstant())) {
                    possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
                }
                break;
            case NumericConstant::Type::Float:
                if (canBeParsedAsRamFloat(constant.getConstant())) {
                    possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
                }
                break;
        }
    } else {
        // Else: all numeric types that can be parsed are valid.
        if (canBeParsedAsRamSigned(constant.getConstant())) {
            possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
        }

        if (canBeParsedAsRamUnsigned(constant.getConstant())) {
            possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
        }

        if (canBeParsedAsRamFloat(constant.getConstant())) {
            possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
        }
    }

    addConstraint(hasSuperTypeInSet(getVar(constant), possibleTypes));
}

void TypeConstraintsAnalysis::visit_(type_identity<BinaryConstraint>, const BinaryConstraint& rel) {
    auto lhs = getVar(rel.getLHS());
    auto rhs = getVar(rel.getRHS());
    addConstraint(isSubtypeOf(lhs, rhs));
    addConstraint(isSubtypeOf(rhs, lhs));
}

void TypeConstraintsAnalysis::visit_(type_identity<IntrinsicFunctor>, const IntrinsicFunctor& fun) {
    auto functorVar = getVar(fun);

    auto argVars = map(fun.getArguments(), [&](auto&& x) { return getVar(x); });
    // The type of the user-defined function might not be set at this stage.
    // If so then add overloads as alternatives
    if (!typeAnalysis.hasValidTypeInfo(fun))
        addConstraint(satisfiesOverload(typeEnv, functorBuiltIn(fun.getBaseFunctionOp()), functorVar, argVars,
                isInfixFunctorOp(fun.getBaseFunctionOp())));

    // In polymorphic case
    // We only require arguments to share a base type with a return type.
    // (instead of, for example, requiring them to be of the same type)
    // This approach is related to old type semantics
    // See #1296 and tests/semantic/type_system4
    if (isInfixFunctorOp(fun.getBaseFunctionOp())) {
        for (auto&& var : argVars)
            addConstraint(subtypesOfTheSameBaseType(var, functorVar));

        return;
    }

    if (!typeAnalysis.hasValidTypeInfo(fun)) {
        return;
    }

    // add a constraint for the return type of the functor
    TypeAttribute returnType = typeAnalysis.getFunctorReturnTypeAttribute(fun);
    addConstraint(isSubtypeOf(functorVar, typeEnv.getConstantType(returnType)));
    // Special case. Ord returns the ram representation of any object.
    if (typeAnalysis.getPolymorphicOperator(fun) == FunctorOp::ORD) {
        return;
    }

    // Add constraints on arguments
    auto arguments = fun.getArguments();
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        TypeAttribute argType = typeAnalysis.getFunctorParamTypeAttribute(fun, i);
        addConstraint(isSubtypeOf(getVar(arguments[i]), typeEnv.getConstantType(argType)));
    }
}

void TypeConstraintsAnalysis::visit_(type_identity<UserDefinedFunctor>, const UserDefinedFunctor& fun) {
    auto functorVar = getVar(fun);

    // I found this very confusing, hopefully this comment helps someone else.
    // I assumed that this branch cannot be taken, because the Semantic checker
    // verifies that every functor has a declaration!  However, it turns out that
    // the SemanticChecker is *not* the first Transformer which gets run and so
    // it's not really clear that those invariants hold yet!
    // I don't particularly like that but am not at a place where I can change the
    // order of the passes/transformers.  So, for now, here's a comment for the next
    // person going doing this rabbit hole.
    auto const& arguments = fun.getArguments();
    if (!typeAnalysis.hasValidTypeInfo(fun) || typeAnalysis.getFunctorArity(fun) != arguments.size()) {
        return;
    }

    // add a constraint for the return type of the functor
    Type const& returnType = typeAnalysis.getFunctorReturnType(fun);
    addConstraint(isSubtypeOf(functorVar, returnType));

    // Add constraints on arguments
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        Type const& paramType = typeAnalysis.getFunctorParamType(fun, i);
        addConstraint(isSubtypeOf(getVar(arguments[i]), paramType));
    }
}

void TypeConstraintsAnalysis::visit_(type_identity<Counter>, const Counter& counter) {
    addConstraint(isSubtypeOf(getVar(counter), typeEnv.getConstantType(TypeAttribute::Signed)));
}

void TypeConstraintsAnalysis::visit_(type_identity<TypeCast>, const ast::TypeCast& typeCast) {
    auto& typeName = typeCast.getType();
    if (!typeEnv.isType(typeName)) {
        return;
    }

    addConstraint(isSubtypeOf(getVar(typeCast), typeEnv.getType(typeName)));

    // If we are dealing with a constant then its type must be deduced from the cast
    // Otherwise, expression like: to_string(as(2, float)) couldn't be typed.
    auto* value = typeCast.getValue();

    if (isA<Constant>(value)) {
        addConstraint(isSubtypeOf(getVar(*value), typeEnv.getType(typeName)));
    }
}

void TypeConstraintsAnalysis::visit_(type_identity<RecordInit>, const RecordInit& record) {
    auto arguments = record.getArguments();
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        addConstraint(isSubtypeOfComponent(getVar(arguments[i]), getVar(record), i));
    }
}

void TypeConstraintsAnalysis::visit_(type_identity<BranchInit>, const BranchInit& adt) {
    auto* correspondingType = sumTypesBranches.getType(adt.getBranchName());

    if (correspondingType == nullptr) {
        return;  // malformed program.
    }

    // Sanity check
    assert(isA<AlgebraicDataType>(correspondingType));

    // Constraint on the whole branch. $Branch(...) <: ADTtype
    addConstraint(isSubtypeOf(getVar(adt), *correspondingType));

    // Even if the branch was declared,
    // it could be that the corresponding branch doesn't exist in the type environment.
    // This can happen when the branch was declared over the invalid type.
    try {
        // Constraints on arguments
        auto branchTypes = as<AlgebraicDataType>(correspondingType)->getBranchTypes(adt.getBranchName());
        auto branchArgs = adt.getArguments();

        if (branchTypes.size() != branchArgs.size()) {
            // invalid program - handled by semantic checker later.
            return;
        }

        // Add constraints for each of the branch arguments.
        for (std::size_t i = 0; i < branchArgs.size(); ++i) {
            auto argVar = getVar(branchArgs[i]);
            addConstraint(isSubtypeOf(argVar, *branchTypes[i]));
        }
    } catch (...) {
        // malformed program - reported by semantic checker.
    }
}

void TypeConstraintsAnalysis::visit_(type_identity<Aggregator>, const Aggregator& agg) {
    if (agg.getBaseOperator() == AggregateOp::COUNT) {
        addConstraint(isSubtypeOf(getVar(agg), typeEnv.getConstantType(TypeAttribute::Signed)));
    } else if (agg.getBaseOperator() == AggregateOp::MEAN) {
        addConstraint(isSubtypeOf(getVar(agg), typeEnv.getConstantType(TypeAttribute::Float)));
    } else {
        addConstraint(hasSuperTypeInSet(getVar(agg), typeEnv.getConstantNumericTypes()));
    }

    // If there is a target expression - it should be of the same type as the aggregator.
    if (auto expr = agg.getTargetExpression()) {
        addConstraint(isSubtypeOf(getVar(expr), getVar(agg)));
        addConstraint(isSubtypeOf(getVar(agg), getVar(expr)));
    }
}

void TypeConstraintsAnalysis::iterateOverAtom(
        const Atom& atom, std::function<void(const Argument&, const Type&)> map) {
    // get relation
    auto rel = getAtomRelation(&atom, &program);
    if (rel == nullptr) {
        return;  // error in input program
    }

    auto atts = rel->getAttributes();
    auto args = atom.getArguments();
    if (atts.size() != args.size()) {
        return;  // error in input program
    }

    for (std::size_t i = 0; i < atts.size(); i++) {
        const auto& typeName = atts[i]->getTypeName();
        if (typeEnv.isType(typeName)) {
            map(*args[i], typeEnv.getType(typeName));
        }
    }
}

void TypeConstraintsAnalysis::collectConstraints(const Clause& clause) {
    sinks.insert(clause.getHead());
    visit(clause, *this);
}

}  // namespace souffle::ast::analysis
