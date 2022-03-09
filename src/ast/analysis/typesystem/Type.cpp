/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Type.cpp
 *
 * Implements a collection of type analyses operating on AST constructs.
 *
 ***********************************************************************/

#include "ast/analysis/typesystem/Type.h"
#include "AggregateOp.h"
#include "FunctorOps.h"
#include "Global.h"
#include "ast/Aggregator.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/BranchInit.h"
#include "ast/Clause.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Negation.h"
#include "ast/NumericConstant.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/TypeCast.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "ast/analysis/Constraint.h"
#include "ast/analysis/ConstraintSystem.h"
#include "ast/analysis/typesystem/SumTypeBranches.h"
#include "ast/analysis/typesystem/TypeConstrainsAnalysis.h"
#include "ast/analysis/typesystem/TypeConstraints.h"
#include "ast/analysis/typesystem/TypeEnvironment.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "ast/utility/Utils.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
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

namespace souffle::ast::analysis {

Own<Clause> TypeAnalysis::createAnnotatedClause(
        const Clause* clause, const std::map<const Argument*, TypeSet> argumentTypes) {
    // Annotates each variable with its type based on a given type analysis result
    struct TypeAnnotator : public NodeMapper {
        const std::map<const Argument*, TypeSet>& types;

        TypeAnnotator(const std::map<const Argument*, TypeSet>& types) : types(types) {}

        Own<Node> operator()(Own<Node> node) const override {
            if (auto* var = as<ast::Variable>(node)) {
                std::stringstream newVarName;
                newVarName << var->getName() << "&isin;" << types.find(var)->second;
                return mk<ast::Variable>(newVarName.str());
            } else if (auto* var = as<UnnamedVariable>(node)) {
                std::stringstream newVarName;
                newVarName << "_"
                           << "&isin;" << types.find(var)->second;
                return mk<ast::Variable>(newVarName.str());
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
    auto annotatedClause = clone(clause);

    // Maps x -> y, where x is the address of an argument in the original clause, and y
    // is the address of the equivalent argument in the clone.
    std::map<const Argument*, const Argument*> memoryMap;

    std::vector<const Argument*> originalAddresses;
    visit(*clause, [&](const Argument& arg) { originalAddresses.push_back(&arg); });

    std::vector<const Argument*> cloneAddresses;
    visit(*annotatedClause, [&](const Argument& arg) { cloneAddresses.push_back(&arg); });

    assert(cloneAddresses.size() == originalAddresses.size());

    for (std::size_t i = 0; i < originalAddresses.size(); i++) {
        memoryMap[originalAddresses[i]] = cloneAddresses[i];
    }

    // Map the types to the clause clone
    std::map<const Argument*, TypeSet> cloneArgumentTypes;
    for (auto& pair : argumentTypes) {
        cloneArgumentTypes[memoryMap[pair.first]] = pair.second;
    }

    // Create the type-annotated clause
    TypeAnnotator annotator(cloneArgumentTypes);
    annotatedClause->apply(annotator);
    return annotatedClause;
}

std::map<const Argument*, TypeSet> TypeAnalysis::analyseTypes(
        const TranslationUnit& tu, const Clause& clause, std::ostream* logs) {
    return TypeConstraintsAnalysis(tu).analyse(clause, logs);
}

void TypeAnalysis::print(std::ostream& os) const {
    os << "-- Analysis logs --" << std::endl;
    os << analysisLogs.str() << std::endl;
    os << "-- Result --" << std::endl;
    for (auto& cur : annotatedClauses) {
        os << *cur << std::endl;
    }

    TypeAnnotationPrinter printer = TypeAnnotationPrinter(translationUnit, argumentTypes, os);
    os << std::endl << "-- Result (2) --" << std::endl;
    auto& program = translationUnit->getProgram();
    for (auto& cur : program.getClauses()) {
        printer.printAnnotatedClause(*cur);
    }
}

Type const& TypeAnalysis::nameToType(QualifiedName const& name) const {
    assert(typeEnv);
    return typeEnv->getType(name);
}

TypeAttribute TypeAnalysis::nameToTypeAttribute(QualifiedName const& name) const {
    return getTypeAttribute(nameToType(name));
}

TypeAttribute TypeAnalysis::getFunctorReturnTypeAttribute(const Functor& functor) const {
    assert(hasValidTypeInfo(functor) && "type of functor not processed");
    if (auto* intrinsic = as<IntrinsicFunctor>(functor)) {
        return functorInfo.at(intrinsic)->result;
    } else if (const auto* udf = as<UserDefinedFunctor>(functor)) {
        return getTypeAttribute(getFunctorReturnType(*udf));
    }
    fatal("Missing functor type.");
}

Type const& TypeAnalysis::getFunctorReturnType(const UserDefinedFunctor& functor) const {
    return nameToType(functorAnalysis->getFunctorReturnType(functor));
}

Type const& TypeAnalysis::getFunctorParamType(const UserDefinedFunctor& functor, std::size_t idx) const {
    return nameToType(functorAnalysis->getFunctorDeclaration(functor).getParams().at(idx)->getTypeName());
}

TypeAttribute TypeAnalysis::getFunctorParamTypeAttribute(const Functor& functor, std::size_t idx) const {
    assert(hasValidTypeInfo(functor) && "type of functor not processed");
    if (auto* intrinsic = as<IntrinsicFunctor>(functor)) {
        auto* info = functorInfo.at(intrinsic);
        return info->params.at(info->variadic ? 0 : idx);
    } else if (auto* udf = as<UserDefinedFunctor>(functor)) {
        return getTypeAttribute(getFunctorParamType(*udf, idx));
    }
    fatal("Missing functor type.");
}

std::vector<TypeAttribute> TypeAnalysis::getFunctorParamTypeAttributes(
        const UserDefinedFunctor& functor) const {
    assert(hasValidTypeInfo(functor) && "type of functor not processed");
    auto const& decl = functorAnalysis->getFunctorDeclaration(functor);
    std::vector<TypeAttribute> res;
    res.reserve(decl.getArity());
    auto const& params = decl.getParams();
    std::transform(params.begin(), params.end(), std::back_inserter(res),
            [this](auto const& attr) { return nameToTypeAttribute(attr->getTypeName()); });
    return res;
}

const std::map<const NumericConstant*, NumericConstant::Type>& TypeAnalysis::getNumericConstantTypes() const {
    return numericConstantType;
}

std::set<TypeAttribute> TypeAnalysis::getTypeAttributes(const Argument* arg) const {
    std::set<TypeAttribute> typeAttributes;

    if (const auto* inf = as<IntrinsicFunctor>(arg)) {
        // intrinsic functor type is its return type if its set
        if (hasValidTypeInfo(*inf)) {
            typeAttributes.insert(getFunctorReturnTypeAttribute(*inf));
            return typeAttributes;
        }
    } else if (const auto* udf = as<UserDefinedFunctor>(arg)) {
        if (hasValidTypeInfo(*udf)) {
            typeAttributes.insert(getFunctorReturnTypeAttribute(*udf));
            return typeAttributes;
        }
    }

    const auto& types = getTypes(arg);
    if (types.isAll()) {
        return {TypeAttribute::Signed, TypeAttribute::Unsigned, TypeAttribute::Float, TypeAttribute::Symbol,
                TypeAttribute::Record};
    }
    for (const auto& type : types) {
        typeAttributes.insert(getTypeAttribute(skipAliasesType(type)));
    }
    return typeAttributes;
}

IntrinsicFunctors TypeAnalysis::getValidIntrinsicFunctorOverloads(const IntrinsicFunctor& inf) const {
    // Get the info of all possible functors which can be used here
    IntrinsicFunctors functorInfos = contains(functorInfo, &inf) ? functorBuiltIn(functorInfo.at(&inf)->op)
                                                                 : functorBuiltIn(inf.getBaseFunctionOp());

    // Filter out the ones which don't fit in with the current knowledge
    auto returnTypes = getTypeAttributes(&inf);
    auto argTypes = map(inf.getArguments(), [&](const Argument* arg) { return getTypeAttributes(arg); });
    auto isValidOverload = [&](const IntrinsicFunctorInfo& candidate) {
        // Check for arity mismatch
        if (!candidate.variadic && argTypes.size() != candidate.params.size()) {
            return false;
        }

        // Check that argument types match
        for (std::size_t i = 0; i < argTypes.size(); ++i) {
            const auto& expectedType = candidate.params[candidate.variadic ? 0 : i];
            if (!contains(argTypes[i], expectedType)) {
                return false;
            }
        }

        // Check that the return type matches
        return contains(returnTypes, candidate.result);
    };
    auto candidates = filter(functorInfos, isValidOverload);

    // Sort them in a standardised way (so order is deterministic)
    auto comparator = [&](const IntrinsicFunctorInfo& a, const IntrinsicFunctorInfo& b) {
        if (a.result != b.result) return a.result < b.result;
        if (a.variadic != b.variadic) return a.variadic < b.variadic;
        return std::lexicographical_compare(
                a.params.begin(), a.params.end(), b.params.begin(), b.params.end());
    };
    std::sort(candidates.begin(), candidates.end(), comparator);

    return candidates;
}

bool TypeAnalysis::hasValidTypeInfo(const Argument& argument) const {
    if (auto* inf = as<IntrinsicFunctor>(argument)) {
        return contains(functorInfo, inf);
    } else if (auto* udf = as<UserDefinedFunctor>(argument)) {
        try {
            auto const& declaration = functorAnalysis->getFunctorDeclaration(*udf);
            return hasValidTypeInfo(declaration);
        } catch (...) {  // functor hasn't been declared
            return false;
        }
    } else if (auto* nc = as<NumericConstant>(argument)) {
        return contains(numericConstantType, nc);
    } else if (auto* agg = as<Aggregator>(argument)) {
        return contains(aggregatorType, agg);
    }
    return true;
}

bool TypeAnalysis::hasValidTypeInfo(const FunctorDeclaration& decl) const {
    auto isValidType = [&](Attribute const& attr) { return typeEnv->isType(attr.getTypeName()); };

    auto const& params = decl.getParams();
    return isValidType(decl.getReturnType()) &&
           std::all_of(
                   params.begin(), params.end(), [&](auto const& attrPtr) { return isValidType(*attrPtr); });
}

NumericConstant::Type TypeAnalysis::getPolymorphicNumericConstantType(const NumericConstant& nc) const {
    assert(hasValidTypeInfo(nc) && "numeric constant type not set");
    return numericConstantType.at(&nc);
}

BinaryConstraintOp TypeAnalysis::getPolymorphicOperator(const BinaryConstraint& bc) const {
    assert(contains(constraintType, &bc) && "binary constraint operator not set");
    return constraintType.at(&bc);
}

AggregateOp TypeAnalysis::getPolymorphicOperator(const Aggregator& agg) const {
    assert(hasValidTypeInfo(agg) && "aggregator operator not set");
    return aggregatorType.at(&agg);
}

FunctorOp TypeAnalysis::getPolymorphicOperator(const IntrinsicFunctor& inf) const {
    assert(hasValidTypeInfo(inf) && "functor type not set");
    return functorInfo.at(&inf)->op;
}

bool TypeAnalysis::analyseIntrinsicFunctors(const TranslationUnit& translationUnit) {
    bool changed = false;
    const auto& program = translationUnit.getProgram();
    visit(program, [&](const IntrinsicFunctor& functor) {
        auto candidates = getValidIntrinsicFunctorOverloads(functor);
        if (candidates.empty()) {
            // No valid overloads - mark it as an invalid functor
            if (contains(functorInfo, &functor)) {
                functorInfo.erase(&functor);
                changed = true;
            }
            return;
        }

        // Update to the canonic representation if different
        const auto* curInfo = &candidates.front().get();
        if (contains(functorInfo, &functor) && functorInfo.at(&functor) == curInfo) {
            return;
        }
        functorInfo[&functor] = curInfo;
        changed = true;
    });
    return changed;
}

bool TypeAnalysis::analyseNumericConstants(const TranslationUnit& translationUnit) {
    bool changed = false;
    const auto& program = translationUnit.getProgram();

    auto setNumericConstantType = [&](const NumericConstant& nc, NumericConstant::Type ncType) {
        if (contains(numericConstantType, &nc) && numericConstantType.at(&nc) == ncType) {
            return;
        }
        changed = true;
        numericConstantType[&nc] = ncType;
    };

    visit(program, [&](const NumericConstant& numericConstant) {
        // Constant has a fixed type
        if (numericConstant.getFixedType().has_value()) {
            setNumericConstantType(numericConstant, numericConstant.getFixedType().value());
            return;
        }

        // Otherwise, type should be inferred
        TypeSet types = getTypes(&numericConstant);
        auto hasOfKind = [&](TypeAttribute kind) -> bool {
            return any_of(types, [&](const analysis::Type& type) { return isOfKind(type, kind); });
        };
        if (hasOfKind(TypeAttribute::Signed)) {
            setNumericConstantType(numericConstant, NumericConstant::Type::Int);
        } else if (hasOfKind(TypeAttribute::Unsigned)) {
            setNumericConstantType(numericConstant, NumericConstant::Type::Uint);
        } else if (hasOfKind(TypeAttribute::Float)) {
            setNumericConstantType(numericConstant, NumericConstant::Type::Float);
        } else {
            // Type information no longer valid
            if (contains(numericConstantType, &numericConstant)) {
                numericConstantType.erase(&numericConstant);
                changed = true;
            }
        }
    });

    return changed;
}

bool TypeAnalysis::analyseAggregators(const TranslationUnit& translationUnit) {
    bool changed = false;
    const auto& program = translationUnit.getProgram();

    auto setAggregatorType = [&](const Aggregator& agg, TypeAttribute attr) {
        auto overloadedType = convertOverloadedAggregator(agg.getBaseOperator(), attr);
        if (contains(aggregatorType, &agg) && aggregatorType.at(&agg) == overloadedType) {
            return;
        }
        changed = true;
        aggregatorType[&agg] = overloadedType;
    };

    visit(program, [&](const Aggregator& agg) {
        if (isOverloadedAggregator(agg.getBaseOperator())) {
            auto* targetExpression = agg.getTargetExpression();
            if (isFloat(targetExpression)) {
                setAggregatorType(agg, TypeAttribute::Float);
            } else if (isUnsigned(targetExpression)) {
                setAggregatorType(agg, TypeAttribute::Unsigned);
            } else {
                setAggregatorType(agg, TypeAttribute::Signed);
            }
        } else {
            if (contains(aggregatorType, &agg)) {
                assert(aggregatorType.at(&agg) == agg.getBaseOperator() &&
                        "non-overloaded aggr types should always be the base operator");
                return;
            }
            changed = true;
            aggregatorType[&agg] = agg.getBaseOperator();
        }
    });

    return changed;
}

bool TypeAnalysis::analyseBinaryConstraints(const TranslationUnit& translationUnit) {
    bool changed = false;
    const auto& program = translationUnit.getProgram();

    auto setConstraintType = [&](const BinaryConstraint& bc, TypeAttribute attr) {
        auto overloadedType = convertOverloadedConstraint(bc.getBaseOperator(), attr);
        if (contains(constraintType, &bc) && constraintType.at(&bc) == overloadedType) {
            return;
        }
        changed = true;
        constraintType[&bc] = overloadedType;
    };

    visit(program, [&](const BinaryConstraint& binaryConstraint) {
        if (isOverloaded(binaryConstraint.getBaseOperator())) {
            // Get arguments
            auto* leftArg = binaryConstraint.getLHS();
            auto* rightArg = binaryConstraint.getRHS();

            // Both args must be of the same type
            if (isFloat(leftArg) && isFloat(rightArg)) {
                setConstraintType(binaryConstraint, TypeAttribute::Float);
            } else if (isUnsigned(leftArg) && isUnsigned(rightArg)) {
                setConstraintType(binaryConstraint, TypeAttribute::Unsigned);
            } else if (isSymbol(leftArg) && isSymbol(rightArg)) {
                setConstraintType(binaryConstraint, TypeAttribute::Symbol);
            } else {
                setConstraintType(binaryConstraint, TypeAttribute::Signed);
            }
        } else {
            if (contains(constraintType, &binaryConstraint)) {
                assert(constraintType.at(&binaryConstraint) == binaryConstraint.getBaseOperator() &&
                        "unexpected constraint type");
                return;
            }
            changed = true;
            constraintType[&binaryConstraint] = binaryConstraint.getBaseOperator();
        }
    });

    return changed;
}

bool TypeAnalysis::isFloat(const Argument* argument) const {
    return isOfKind(getTypes(argument), TypeAttribute::Float);
}

bool TypeAnalysis::isUnsigned(const Argument* argument) const {
    return isOfKind(getTypes(argument), TypeAttribute::Unsigned);
}

bool TypeAnalysis::isSymbol(const Argument* argument) const {
    return isOfKind(getTypes(argument), TypeAttribute::Symbol);
}

void TypeAnalysis::run(const TranslationUnit& translationUnit) {
    // Check if debugging information is being generated
    std::ostream* debugStream = nullptr;
    if (Global::config().has("debug-report") || Global::config().has("show", "type-analysis")) {
        debugStream = &analysisLogs;
    }

    typeEnv = &translationUnit.getAnalysis<TypeEnvironmentAnalysis>().getTypeEnvironment();
    functorAnalysis = &translationUnit.getAnalysis<FunctorAnalysis>();

    // Analyse user-defined functor types
    const Program& program = translationUnit.getProgram();

    // Rest of the analysis done until fixpoint reached
    bool changed = true;
    while (changed) {
        changed = false;
        argumentTypes.clear();

        // Analyse general argument types, clause by clause.
        for (const Clause* clause : program.getClauses()) {
            auto clauseArgumentTypes = analyseTypes(translationUnit, *clause, debugStream);
            argumentTypes.insert(clauseArgumentTypes.begin(), clauseArgumentTypes.end());

            if (debugStream != nullptr) {
                // Store an annotated clause for printing purposes
                annotatedClauses.emplace_back(createAnnotatedClause(clause, clauseArgumentTypes));
            }
        }

        // Analyse intrinsic-functor types
        changed |= analyseIntrinsicFunctors(translationUnit);

        // Deduce numeric-constant polymorphism
        changed |= analyseNumericConstants(translationUnit);

        // Deduce aggregator polymorphism
        changed |= analyseAggregators(translationUnit);

        // Deduce binary-constraint polymorphism
        changed |= analyseBinaryConstraints(translationUnit);
    }

    // For refined type annotation printing
    this->translationUnit = &translationUnit;
}

void TypeAnnotationPrinter::branchOnArgument(const Argument* cur, const Type& type) {
    if (isA<ast::Variable>(*cur)) {
        auto var = as<ast::Variable>(cur);
        auto inferedTypeName = argumentTypes.find(var)->second;
        os << *as<ast::Variable>(cur) << "∈" << inferedTypeName;
    } else if (isA<UnnamedVariable>(*cur)) {
        os << "_";
    } else if (isA<NumericConstant>(*cur)) {
        print_(type_identity<NumericConstant>(), *as<NumericConstant>(cur));
    } else if (isA<StringConstant>(*cur)) {
        print_(type_identity<StringConstant>(), *as<StringConstant>(cur));
    } else if (isA<NilConstant>(*cur)) {
        print_(type_identity<NilConstant>(), *as<NilConstant>(cur));
    } else if (isA<RecordInit>(*cur)) {
        print_(type_identity<RecordInit>(), *as<RecordInit>(cur), *as<RecordType>(getBaseType(&type)));
    } else if (isA<BranchInit>(*cur)) {
        print_(type_identity<BranchInit>(), *as<BranchInit>(cur));
    } else if (isA<IntrinsicFunctor>(*cur)) {
        print_(type_identity<IntrinsicFunctor>(), *as<IntrinsicFunctor>(cur));
    } else if (isA<TypeCast>(*cur)) {
        print_(type_identity<TypeCast>(), *as<TypeCast>(cur));
    } else if (isA<UserDefinedFunctor>(*cur)) {
        print_(type_identity<UserDefinedFunctor>(), *as<UserDefinedFunctor>(cur));
    } else if (isA<Counter>(*cur)) {
        print_(type_identity<Counter>(), *as<Counter>(cur));
    } else if (isA<Aggregator>(*cur)) {
        print_(type_identity<Aggregator>(), *as<Aggregator>(cur));
    } else {
        os << "<(branchOnArgument) not supported yet>";
    }
}

void TypeAnnotationPrinter::print_(type_identity<Atom>, const Atom& atom) {
    auto name = atom.getQualifiedName();
    os << name << "(";
    auto args = atom.getArguments();
    auto rel = program.getRelation(atom);
    auto atts = rel->getAttributes();

    std::size_t i = 0;
    for (auto cur : args) {
        const auto& declaredTypeName = atts[i]->getTypeName();
        const auto& declaredType = typeEnv.getType(declaredTypeName);
        assert(typeEnv.isType(declaredTypeName));
        branchOnArgument(cur, declaredType);
        if (isA<RecordInit>(cur) || isA<UnnamedVariable>(cur) || isA<TypeCast>(cur)) {
            os << "∈{" << declaredTypeName << "}";
        }
        if (i + 1 < args.size()) {
            os << ",";
        }
        i++;
    }

    os << ")";
}

void TypeAnnotationPrinter::print_(type_identity<Negation>, const Negation& cur) {
    os << "!";
    print_(type_identity<Atom>(), *cur.getAtom());
}

void TypeAnnotationPrinter::print_(type_identity<NilConstant>, const NilConstant& nilCnst) {
    os << nilCnst << "∈{any_record}";
}

void TypeAnnotationPrinter::print_(type_identity<StringConstant>, const StringConstant& cnst) {
    os << cnst << "∈{string}";
}

void TypeAnnotationPrinter::print_(type_identity<NumericConstant>, const NumericConstant& constant) {
    os << constant;
    auto numericConstantTypes = typeAnalysis.getNumericConstantTypes();
    auto numType = numericConstantTypes.find(&constant)->second;
    if (numType == NumericConstant::Type::Int) {
        os << "∈{Int}";
    } else if (numType == NumericConstant::Type::Uint) {
        os << "∈{Uint}";
    } else if (numType == NumericConstant::Type::Float) {
        os << "∈{Float}";
    } else {
        os << "∈{???}";
    }
}

void TypeAnnotationPrinter::print_(type_identity<BinaryConstraint>, const BinaryConstraint& rel) {
    auto lhs = rel.getLHS();
    auto lTySet = argumentTypes.find(lhs)->second;
    assert(lTySet.size() == 1);
    auto lTy = lTySet.begin();
    branchOnArgument(lhs, *lTy);
    if (isA<RecordInit>(lhs) || isA<UnnamedVariable>(lhs)) {
        os << "∈{" << lTy->getName() << "}";
    }

    os << " " << rel.getBaseOperator() << " ";

    auto rhs = rel.getRHS();
    auto rTySet = argumentTypes.find(rhs)->second;
    assert(rTySet.size() == 1);
    auto rTy = rTySet.begin();
    branchOnArgument(rhs, *rTy);
    if (isA<RecordInit>(rhs) || isA<UnnamedVariable>(rhs)) {
        os << "∈{" << rTy->getName() << "}";
    }
}

void TypeAnnotationPrinter::print_(type_identity<IntrinsicFunctor>, const IntrinsicFunctor& fun) {
    auto arguments = fun.getArguments();
    if (arguments.size() == 2) {  // binary
        auto tySet = argumentTypes.find(arguments[0])->second;
        assert(tySet.size() == 1);
        auto ty = tySet.begin();
        os << "(";
        branchOnArgument(arguments[0], *ty);
        if (isA<RecordInit>(arguments[0]) || isA<UnnamedVariable>(arguments[0])) {
            os << "∈{" << ty->getName() << "}";
        }

        os << " " << fun.getBaseFunctionOp() << " ";

        auto tySet2 = argumentTypes.find(arguments[1])->second;
        assert(tySet2.size() == 1);
        auto ty2 = tySet2.begin();
        branchOnArgument(arguments[1], *ty2);
        if (isA<RecordInit>(arguments[1]) || isA<UnnamedVariable>(arguments[1])) {
            os << "∈{" << ty2->getName() << "}";
        }

        os << ")";
    } else {
        os << fun.getBaseFunctionOp() << "(";
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            TypeAttribute argType = typeAnalysis.getFunctorParamTypeAttribute(fun, i);
            auto& ty = typeEnv.getConstantType(argType);
            branchOnArgument(arguments[i], ty);
            if (i + 1 < arguments.size()) {
                os << ",";
            }
        }
        os << ")";
    }
}

void TypeAnnotationPrinter::print_(type_identity<UserDefinedFunctor>, const UserDefinedFunctor& fun) {
    auto arguments = fun.getArguments();
    os << "@" << fun.getName() << "(";
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        TypeAttribute argType = typeAnalysis.getFunctorParamTypeAttribute(fun, i);
        auto& ty = typeEnv.getConstantType(argType);
        branchOnArgument(arguments[i], ty);
        if (i + 1 < arguments.size()) {
            os << ",";
        }
    }
    os << ")";
}

void TypeAnnotationPrinter::print_(type_identity<Counter>, [[maybe_unused]] const Counter& counter) {
    os << "$∈{number}";
}

void TypeAnnotationPrinter::print_(type_identity<TypeCast>, const ast::TypeCast& typeCast) {
    os << "as(";
    auto& ty = typeEnv.getType(typeCast.getType());
    branchOnArgument(typeCast.getValue(), ty);
    os << "," << ty.getName() << ")";
}

void TypeAnnotationPrinter::print_(
        type_identity<RecordInit>, const RecordInit& record, const RecordType& type) {
    auto arguments = record.getArguments();
    auto& ftypes = type.getFields();
    os << "[";
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        branchOnArgument(arguments[i], *ftypes[i]);
        if (isA<RecordInit>(arguments[i]) || isA<UnnamedVariable>(arguments[i])) {
            os << "∈{" << (*ftypes[i]).getName() << "}";
        }
        if (i + 1 < arguments.size()) {
            os << ",";
        }
    }
    os << "]";
}

void TypeAnnotationPrinter::print_(type_identity<BranchInit>, const BranchInit& adt) {
    auto* correspondingType = sumTypesBranches.getType(adt.getBranchName());

    assert(correspondingType != nullptr);
    assert(isA<AlgebraicDataType>(correspondingType));

    auto branchTypes = as<AlgebraicDataType>(correspondingType)->getBranchTypes(adt.getBranchName());
    auto branchArgs = adt.getArguments();

    assert(branchTypes.size() == branchArgs.size());

    os << "$" << adt.getBranchName() << "(";
    for (std::size_t i = 0; i < branchArgs.size(); i++) {
        auto arg = branchArgs[i];
        auto argTy = branchTypes[i];
        branchOnArgument(arg, *argTy);
        if (isA<RecordInit>(arg) || isA<UnnamedVariable>(arg)) {
            os << "∈{" << argTy->getName() << "}";
        }
        if (i + 1 < branchArgs.size()) {
            os << ", ";
        }
    }
    os << ")";
}

void TypeAnnotationPrinter::print_(type_identity<Aggregator>, const Aggregator& agg) {
    auto baseOperator = agg.getBaseOperator();
    auto bodyLiterals = agg.getBodyLiterals();
    os << baseOperator << " ";
    auto targetExpr = agg.getTargetExpression();
    if (targetExpr /* the target expression can be null */) {
        auto tySet = argumentTypes.find(targetExpr)->second;
        assert(tySet.size() == 1);
        auto ty = tySet.begin();
        branchOnArgument(targetExpr, *ty);
    }
    os << " : { ";
    printBodyLiterals(bodyLiterals, "        ");
    os << " }";
}

void TypeAnnotationPrinter::printBodyLiterals(std::vector<Literal*> bodyLiterals, const std::string& spc) {
    std::size_t i = 0;
    for (Literal* cur : bodyLiterals) {
        if (isA<Atom>(*cur)) {
            print_(type_identity<Atom>(), *as<Atom>(cur));
        } else if (isA<Negation>(*cur)) {
            print_(type_identity<Negation>(), *as<Negation>(*cur));
        } else if (isA<BinaryConstraint>(*cur)) {
            print_(type_identity<BinaryConstraint>(), *as<BinaryConstraint>(*cur));
        } else {
            os << "(?)";
        }

        if (i + 1 < bodyLiterals.size()) {
            os << "," << std::endl;
            // os << "    ";
            os << spc;
        }
        i++;
    }
}

void TypeAnnotationPrinter::printAnnotatedClause(const Clause& clause) {
    Atom* head = clause.getHead();
    print_(type_identity<Atom>(), *head);
    auto bodyLiterals = clause.getBodyLiterals();
    if (bodyLiterals.size() > 0) {
        os << " :- " << std::endl << "    ";
        printBodyLiterals(bodyLiterals, "    ");
    }
    os << "." << std::endl;
}

}  // namespace souffle::ast::analysis
