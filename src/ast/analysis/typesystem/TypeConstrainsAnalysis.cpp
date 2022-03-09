/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeConstraintsAnalysis.cpp
 *
 ***********************************************************************/

#include "ast/analysis/typesystem/TypeConstrainsAnalysis.h"
#include "ast/analysis/typesystem/TypeConstraints.h"

namespace souffle::ast::analysis {

void TypeConstraintsAnalysis::visitSink(const Atom& atom) {
    iterateOverAtom(atom, [&](const Argument& argument, const Type& attributeType) {
        if (isA<RecordType>(skipAliasesType(attributeType))) {
            addConstraint(isSubtypeOf(getVar(argument), attributeType));
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
                possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
                break;
            case NumericConstant::Type::Uint:
                possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
                break;
            case NumericConstant::Type::Float:
                possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
                break;
        }
    } else if (contains(typeAnalysis.getNumericConstantTypes(), &constant)) {
        switch (typeAnalysis.getNumericConstantTypes().at(&constant)) {
            // Insert a type, but only after checking that parsing is possible.
            case NumericConstant::Type::Int:
                possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
                break;
            case NumericConstant::Type::Uint:
                possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
                break;
            case NumericConstant::Type::Float:
                possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
                break;
        }
    } else {
        possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
        possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
        possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
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
    if (!typeAnalysis.hasValidTypeInfo(fun) || functorAnalysis.getFunctorArity(fun) != arguments.size()) {
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
    auto& typeName = adt.getBranchName();
    auto* correspondingType = sumTypesBranches.getType(typeName);

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
        auto& typeName = adt.getBranchName();
        auto branchTypes = as<AlgebraicDataType>(correspondingType)->getBranchTypes(typeName);
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
    auto rel = program.getRelation(atom);
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
