/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypeAnalysis.cpp
 *
 * Implements a collection of type analyses operating on AST constructs.
 *
 ***********************************************************************/

#include "ast/analysis/AstTypeAnalysis.h"
#include "AggregateOp.h"
#include "Constraints.h"
#include "FunctorOps.h"
#include "Global.h"
#include "RamTypes.h"
#include "ast/AstArgument.h"
#include "ast/AstAttribute.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/AstConstraintAnalysis.h"
#include "ast/analysis/AstTypeEnvironmentAnalysis.h"
#include "utility/ContainerUtil.h"
#include "utility/FunctionalUtil.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace souffle {

namespace {

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
using TypeVar = AstConstraintAnalysisVar<type_lattice>;

/** The definition of the type of constraint to be utilized in the type analysis */
using TypeConstraint = std::shared_ptr<Constraint<TypeVar>>;

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
TypeConstraint hasSuperTypeInSet(const TypeVar& var, TypeSet values) {
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

const Type& getBaseType(const Type* type) {
    while (auto subset = dynamic_cast<const SubsetType*>(type)) {
        type = &subset->getBaseType();
    };
    assert((isA<ConstantType>(type) || isA<RecordType>(type)) &&
            "Root must be a constant type or a record type");
    return *type;
}

/**
 * Ensure that types of left and right have the same base types.
 */
TypeConstraint subtypesOfTheSameBaseType(const TypeVar& left, const TypeVar& right) {
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

        bool update(Assignment<TypeVar>& assigment) const override {
            auto subtypesOf = [&](const TypeSet& src, TypeAttribute tyAttr) {
                auto& ty = typeEnv.getConstantType(tyAttr);
                return src.filter(TypeSet(true), [&](auto&& x) { return isSubtypeOf(x, ty); });
            };

            auto possible = [&](TypeAttribute ty, const TypeVar& var) {
                auto& curr = assigment[var];
                return curr.isAll() || any_of(curr, [&](auto&& t) { return getTypeAttribute(t) == ty; });
            };

            overloads = filterNot(std::move(overloads), [&](const IntrinsicFunctor& x) -> bool {
                if (!x.variadic && args.size() != x.params.size()) return true;  // arity mismatch?

                for (size_t i = 0; i < args.size(); ++i)
                    if (!possible(x.params[x.variadic ? 0 : i], args[i])) return true;

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
                    for (size_t i = 0; i < args.size(); ++i) {
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
TypeConstraint isSubtypeOfComponent(
        const TypeVar& elementVariable, const TypeVar& recordVariable, size_t index) {
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
}  // namespace

/* Return a new clause with type-annotated variables */
std::unique_ptr<AstClause> createAnnotatedClause(
        const AstClause* clause, const std::map<const AstArgument*, TypeSet> argumentTypes) {
    // Annotates each variable with its type based on a given type analysis result
    struct TypeAnnotator : public AstNodeMapper {
        const std::map<const AstArgument*, TypeSet>& types;

        TypeAnnotator(const std::map<const AstArgument*, TypeSet>& types) : types(types) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* var = dynamic_cast<AstVariable*>(node.get())) {
                std::stringstream newVarName;
                newVarName << var->getName() << "&isin;" << types.find(var)->second;
                return std::make_unique<AstVariable>(newVarName.str());
            } else if (auto* var = dynamic_cast<AstUnnamedVariable*>(node.get())) {
                std::stringstream newVarName;
                newVarName << "_"
                           << "&isin;" << types.find(var)->second;
                return std::make_unique<AstVariable>(newVarName.str());
            }
            node->apply(*this);
            return node;
        }
    };

    /* Note:
     * Because the type of each argument is stored in the form [address -> type-set],
     * the type-analysis result does not immediately apply to the clone due to differing
     * addresses.
     * Two ways around this:
     *  (1) Perform the type-analysis again for the cloned clause
     *  (2) Keep track of the addresses of equivalent arguments in the cloned clause
     * Method (2) was chosen to avoid having to recompute the analysis each time.
     */
    auto annotatedClause = souffle::clone(clause);

    // Maps x -> y, where x is the address of an argument in the original clause, and y
    // is the address of the equivalent argument in the clone.
    std::map<const AstArgument*, const AstArgument*> memoryMap;

    std::vector<const AstArgument*> originalAddresses;
    visitDepthFirst(*clause, [&](const AstArgument& arg) { originalAddresses.push_back(&arg); });

    std::vector<const AstArgument*> cloneAddresses;
    visitDepthFirst(*annotatedClause, [&](const AstArgument& arg) { cloneAddresses.push_back(&arg); });

    assert(cloneAddresses.size() == originalAddresses.size());

    for (size_t i = 0; i < originalAddresses.size(); i++) {
        memoryMap[originalAddresses[i]] = cloneAddresses[i];
    }

    // Map the types to the clause clone
    std::map<const AstArgument*, TypeSet> cloneArgumentTypes;
    for (auto& pair : argumentTypes) {
        cloneArgumentTypes[memoryMap[pair.first]] = pair.second;
    }

    // Create the type-annotated clause
    TypeAnnotator annotator(cloneArgumentTypes);
    annotatedClause->apply(annotator);
    return annotatedClause;
}

/**
 * Constraint analysis framework for types.
 *
 * The analysis operates on the concept of sinks and sources.
 * If the atom is negated or is a head then it's a sink,
 * and we can only extract the kind constraint from it
 * Otherwise it is a source, and the type of the element must
 * be a subtype of source attribute.
 */
class TypeConstraintsAnalysis : public AstConstraintAnalysis<TypeVar> {
public:
    TypeConstraintsAnalysis(const TypeEnvironment& typeEnv, const AstProgram& program)
            : typeEnv(typeEnv), program(program) {}

private:
    const TypeEnvironment& typeEnv;
    const AstProgram& program;

    // Sinks = {head} ∪ {negated atoms}
    std::set<const AstAtom*> sinks;

    void collectConstraints(const AstClause& clause) override {
        sinks.insert(clause.getHead());
        visitDepthFirstPreOrder(clause, *this);
    }

    void visitSink(const AstAtom& atom) {
        iterateOverAtom(atom, [&](const AstArgument& argument, const Type& attributeType) {
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

    void visitAtom(const AstAtom& atom) override {
        if (contains(sinks, &atom)) {
            visitSink(atom);
            return;
        }

        iterateOverAtom(atom, [&](const AstArgument& argument, const Type& attributeType) {
            addConstraint(isSubtypeOf(getVar(argument), attributeType));
        });
    }

    void visitNegation(const AstNegation& cur) override {
        sinks.insert(cur.getAtom());
    }

    void visitStringConstant(const AstStringConstant& cnst) override {
        addConstraint(isSubtypeOf(getVar(cnst), typeEnv.getConstantType(TypeAttribute::Symbol)));
    }

    void visitNumericConstant(const AstNumericConstant& constant) override {
        TypeSet possibleTypes;

        // Check if the type is given.
        if (constant.getType().has_value()) {
            switch (*constant.getType()) {
                // Insert a type, but only after checking that parsing is possible.
                case AstNumericConstant::Type::Int:
                    if (canBeParsedAsRamSigned(constant.getConstant())) {
                        possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Signed));
                    }
                    break;
                case AstNumericConstant::Type::Uint:
                    if (canBeParsedAsRamUnsigned(constant.getConstant())) {
                        possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Unsigned));
                    }
                    break;
                case AstNumericConstant::Type::Float:
                    if (canBeParsedAsRamFloat(constant.getConstant())) {
                        possibleTypes.insert(typeEnv.getConstantType(TypeAttribute::Float));
                    }
                    break;
            }
            // Else: all numeric types that can be parsed are valid.
        } else {
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

    void visitBinaryConstraint(const AstBinaryConstraint& rel) override {
        auto lhs = getVar(rel.getLHS());
        auto rhs = getVar(rel.getRHS());
        addConstraint(isSubtypeOf(lhs, rhs));
        addConstraint(isSubtypeOf(rhs, lhs));
    }

    void visitFunctor(const AstFunctor& fun) override {
        auto functorVar = getVar(fun);

        auto intrFun = as<AstIntrinsicFunctor>(fun);
        if (intrFun) {
            auto argVars = map(intrFun->getArguments(), [&](auto&& x) { return getVar(x); });
            // The type of the user-defined function might not be set at this stage.
            // If so then add overloads as alternatives
            if (!intrFun->getFunctionInfo())
                addConstraint(satisfiesOverload(typeEnv, functorBuiltIn(intrFun->getFunction()), functorVar,
                        argVars, isInfixFunctorOp(intrFun->getFunction())));

            // In polymorphic case
            // We only require arguments to share a base type with a return type.
            // (instead of, for example, requiring them to be of the same type)
            // This approach is related to old type semantics
            // See #1296 and tests/semantic/type_system4
            if (isInfixFunctorOp(intrFun->getFunction())) {
                for (auto&& var : argVars)
                    addConstraint(subtypesOfTheSameBaseType(var, functorVar));

                return;
            }

            if (!intrFun->getFunctionInfo()) return;
        }

        // add a constraint for the return type of the functor
        addConstraint(isSubtypeOf(functorVar, typeEnv.getConstantType(fun.getReturnType())));

        // Special case. Ord returns the ram representation of any object.
        if (intrFun && intrFun->getFunctionInfo()->op == FunctorOp::ORD) return;

        auto arguments = fun.getArguments();
        for (size_t i = 0; i < arguments.size(); ++i) {
            addConstraint(isSubtypeOf(getVar(arguments[i]), typeEnv.getConstantType(fun.getArgType(i))));
        }
    }

    void visitCounter(const AstCounter& counter) override {
        addConstraint(isSubtypeOf(getVar(counter), typeEnv.getConstantType(TypeAttribute::Signed)));
    }

    void visitTypeCast(const AstTypeCast& typeCast) override {
        auto& typeName = typeCast.getType();
        if (!typeEnv.isType(typeName)) {
            return;
        }

        addConstraint(isSubtypeOf(getVar(typeCast), typeEnv.getType(typeName)));

        // If we are dealing with a constant then its type must be deduced from the cast
        // Otherwise, expression like: to_string(as(2, float)) couldn't be typed.
        auto* value = typeCast.getValue();

        if (isA<AstConstant>(value)) {
            addConstraint(isSubtypeOf(getVar(*value), typeEnv.getType(typeName)));
        }
    }

    void visitRecordInit(const AstRecordInit& record) override {
        auto arguments = record.getArguments();
        for (size_t i = 0; i < arguments.size(); ++i) {
            addConstraint(isSubtypeOfComponent(getVar(arguments[i]), getVar(record), i));
        }
    }

    void visitAggregator(const AstAggregator& agg) override {
        if (agg.getOperator() == AggregateOp::COUNT) {
            addConstraint(isSubtypeOf(getVar(agg), typeEnv.getConstantType(TypeAttribute::Signed)));
        } else if (agg.getOperator() == AggregateOp::MEAN) {
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

    /**
     * Utility function.
     * Iterate over atoms valid pairs of (argument, type-attribute) and apply procedure `map` for its
     * side-effects.
     */
    void iterateOverAtom(const AstAtom& atom, std::function<void(const AstArgument&, const Type&)> map) {
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

        for (size_t i = 0; i < atts.size(); i++) {
            const auto& typeName = atts[i]->getTypeName();
            if (typeEnv.isType(typeName)) {
                map(*args[i], typeEnv.getType(typeName));
            }
        }
    }
};

std::map<const AstArgument*, TypeSet> TypeAnalysis::analyseTypes(const TypeEnvironment& typeEnv,
        const AstClause& clause, const AstProgram& program, std::ostream* logs) {
    return TypeConstraintsAnalysis(typeEnv, program).analyse(clause, logs);
}

void TypeAnalysis::print(std::ostream& os) const {
    os << "-- Analysis logs --" << std::endl;
    os << analysisLogs.str() << std::endl;
    os << "-- Result --" << std::endl;
    for (const auto& cur : annotatedClauses) {
        os << *cur << std::endl;
    }
}

void TypeAnalysis::run(const AstTranslationUnit& translationUnit) {
    // Check if debugging information is being generated
    std::ostream* debugStream = nullptr;
    if (Global::config().has("debug-report") || Global::config().has("show", "type-analysis")) {
        debugStream = &analysisLogs;
    }
    const auto& program = *translationUnit.getProgram();
    auto& typeEnv = translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    // Analyse types, clause by clause.
    for (const AstClause* clause : program.getClauses()) {
        auto clauseArgumentTypes = analyseTypes(typeEnv, *clause, program, debugStream);
        argumentTypes.insert(clauseArgumentTypes.begin(), clauseArgumentTypes.end());

        if (debugStream != nullptr) {
            // Store an annotated clause for printing purposes
            annotatedClauses.emplace_back(createAnnotatedClause(clause, clauseArgumentTypes));
        }
    }
}

}  // end of namespace souffle
