/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeChecker.cpp
 *
 * Implementation of the type checker pass.
 *
 ***********************************************************************/

#include "ast/transform/TypeChecker.h"
#include "Global.h"
#include "ast/Aggregator.h"
#include "ast/AlgebraicDataType.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/BinaryConstraint.h"
#include "ast/BranchInit.h"
#include "ast/BranchType.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/Counter.h"
#include "ast/Functor.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/QualifiedName.h"
#include "ast/analysis/Functor.h"
#include "ast/analysis/typesystem/PolymorphicObjects.h"
#include "ast/analysis/typesystem/SumTypeBranches.h"
#include "ast/analysis/typesystem/Type.h"
#include "ast/analysis/typesystem/TypeEnvironment.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "reports/ErrorReport.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
namespace souffle::ast::transform {

using namespace analysis;

class TypeDeclarationChecker {
public:
    TypeDeclarationChecker(TranslationUnit& tu) : tu(tu){};
    void run();

private:
    TranslationUnit& tu;
    ErrorReport& report = tu.getErrorReport();
    const Program& program = tu.getProgram();
    const TypeEnvironmentAnalysis& typeEnvAnalysis = tu.getAnalysis<TypeEnvironmentAnalysis>();
    const TypeEnvironment& typeEnv = typeEnvAnalysis.getTypeEnvironment();

    void checkRecordType(const ast::RecordType& type);
    void checkSubsetType(const ast::SubsetType& type);
    void checkAliasType(const ast::AliasType& type);
    void checkUnionType(const ast::UnionType& type);
    void checkADT(const ast::AlgebraicDataType& type);
};

class TypeCheckerImpl : public Visitor<void> {
public:
    TypeCheckerImpl(TranslationUnit& tu) : tu(tu){};

    /** Analyse types, clause by clause */
    void run() {
        for (auto const* clause : program.getClauses()) {
            visit(*clause, *this);
        }

        for (auto const* decl : program.getFunctorDeclarations()) {
            if (!typeAnalysis.hasValidTypeInfo(*decl)) {
                // This could happen if the types mentioned int the functor declaration
                // are not valid
                continue;
            }

            // Only stateful functors can use UDTs
            auto goodFunctor = [this, stateful = decl->isStateful()](QualifiedName const& tName) {
                auto attr = getTypeAttribute(typeEnv.getType(tName));
                return stateful || (attr != TypeAttribute::ADT && attr != TypeAttribute::Record);
            };

            if (!goodFunctor(decl->getReturnType().getTypeName())) {
                report.addError(
                        "Functors which are not stateful cannot use UDTs", decl->getReturnType().getSrcLoc());
            }

            for (auto const& param : decl->getParams()) {
                if (!goodFunctor(param->getTypeName())) {
                    report.addError("Functors which are not stateful cannot use UDTs", param->getSrcLoc());
                }
            }
        }
    }

private:
    TranslationUnit& tu;
    ErrorReport& report = tu.getErrorReport();
    const TypeAnalysis& typeAnalysis = tu.getAnalysis<TypeAnalysis>();
    const TypeEnvironment& typeEnv = tu.getAnalysis<TypeEnvironmentAnalysis>().getTypeEnvironment();
    const PolymorphicObjectsAnalysis& polyAnalysis = tu.getAnalysis<PolymorphicObjectsAnalysis>();
    const SumTypeBranchesAnalysis& sumTypesBranches = tu.getAnalysis<SumTypeBranchesAnalysis>();

    const Program& program = tu.getProgram();

    std::unordered_set<const Atom*> negatedAtoms;

    /** Collect negated atoms */
    void visit_(type_identity<Negation>, const Negation& neg) override;

    /* Type checks */
    /** Check if declared types of the relation match deduced types. */
    void visit_(type_identity<Atom>, const Atom& atom) override;
    void visit_(type_identity<Variable>, const Variable& var) override;
    void visit_(type_identity<StringConstant>, const StringConstant& constant) override;
    void visit_(type_identity<NumericConstant>, const NumericConstant& constant) override;
    void visit_(type_identity<NilConstant>, const NilConstant& constant) override;
    void visit_(type_identity<RecordInit>, const RecordInit& rec) override;
    void visit_(type_identity<BranchInit>, const BranchInit& adt) override;
    void visit_(type_identity<TypeCast>, const ast::TypeCast& cast) override;
    void visit_(type_identity<IntrinsicFunctor>, const IntrinsicFunctor& fun) override;
    void visit_(type_identity<UserDefinedFunctor>, const UserDefinedFunctor& fun) override;
    void visit_(type_identity<BinaryConstraint>, const BinaryConstraint& constraint) override;
    void visit_(type_identity<Aggregator>, const Aggregator& aggregator) override;
};  // namespace souffle::ast::transform

void TypeChecker::verify(TranslationUnit& tu) {
    auto& report = tu.getErrorReport();
    auto errorsBeforeDeclarationsCheck = report.getNumErrors();

    TypeDeclarationChecker{tu}.run();

    // Run type checker only if type declarations are valid.
    if (report.getNumErrors() == errorsBeforeDeclarationsCheck) {
        TypeCheckerImpl{tu}.run();
    }
}

void TypeDeclarationChecker::checkUnionType(const ast::UnionType& type) {
    // check presence of all the element types and that all element types are based off a primitive
    for (const QualifiedName& sub : type.getTypes()) {
        if (typeEnv.isPrimitiveType(sub)) {
            continue;
        }
        const ast::Type* subtype = getIf(
                program.getTypes(), [&](const ast::Type* type) { return type->getQualifiedName() == sub; });
        if (subtype == nullptr) {
            report.addError(tfm::format("Undefined type %s in definition of union type %s", sub,
                                    type.getQualifiedName()),
                    type.getSrcLoc());
        } else if (!isA<ast::UnionType>(subtype) && !isA<ast::SubsetType>(subtype) &&
                   !isA<ast::AliasType>(subtype)) {
            report.addError(tfm::format("Union type %s contains the non-primitive type %s",
                                    type.getQualifiedName(), sub),
                    type.getSrcLoc());
        }
    }

    // Check if the union is recursive.
    if (typeEnvAnalysis.isCyclic(type.getQualifiedName())) {
        report.addError("Infinite descent in the definition of type " + toString(type.getQualifiedName()),
                type.getSrcLoc());
    }

    /* check that union types do not mix different primitive types */
    for (const auto* type : program.getTypes()) {
        // We are only interested in unions here.
        if (!isA<ast::UnionType>(type)) {
            continue;
        }

        const auto& name = type->getQualifiedName();

        const auto& predefinedTypesInUnion = typeEnvAnalysis.getPrimitiveTypesInUnion(name);

        // Report error (if size == 0, then the union is cyclic)
        if (predefinedTypesInUnion.size() > 1) {
            report.addError(
                    tfm::format("Union type %s is defined over {%s} (multiple primitive types in union)",
                            name, join(predefinedTypesInUnion, ", ")),
                    type->getSrcLoc());
        }
    }
}

void TypeDeclarationChecker::checkRecordType(const ast::RecordType& type) {
    auto&& fields = type.getFields();
    // check proper definition of all field types
    for (auto&& field : fields) {
        if (!typeEnv.isType(field->getTypeName())) {
            report.addError(tfm::format("Undefined type %s in definition of field %s", field->getTypeName(),
                                    field->getName()),
                    field->getSrcLoc());
        }
    }

    // check that field names are unique
    for (std::size_t i = 0; i < fields.size(); i++) {
        auto&& cur_name = fields[i]->getName();
        for (std::size_t j = 0; j < i; j++) {
            if (fields[j]->getName() == cur_name) {
                report.addError(tfm::format("Doubly defined field name %s in definition of type %s", cur_name,
                                        type.getQualifiedName()),
                        fields[i]->getSrcLoc());
            }
        }
    }
}

void TypeDeclarationChecker::checkADT(const ast::AlgebraicDataType& type) {
    // check if all branches contain properly defined types.
    for (auto* branch : type.getBranches()) {
        for (auto* field : branch->getFields()) {
            if (!typeEnv.isType(field->getTypeName())) {
                report.addError(tfm::format("Undefined type %s in definition of branch %s",
                                        field->getTypeName(), branch->getBranchName()),
                        field->getSrcLoc());
            }
        }
    }
}

void TypeDeclarationChecker::checkAliasType(const ast::AliasType& astType) {
    if (typeEnvAnalysis.isCyclic(astType.getQualifiedName())) {
        report.addError(
                tfm::format("Infinite descent in the definition of type %s", astType.getQualifiedName()),
                astType.getSrcLoc());
        return;
    }
    if (!typeEnv.isType(astType.getAliasType())) {
        report.addError(tfm::format("Undefined alias type %s in definition of type %s",
                                astType.getAliasType(), astType.getQualifiedName()),
                astType.getSrcLoc());
        return;
    }
}

void TypeDeclarationChecker::checkSubsetType(const ast::SubsetType& astType) {
    if (typeEnvAnalysis.isCyclic(astType.getQualifiedName())) {
        report.addError(
                tfm::format("Infinite descent in the definition of type %s", astType.getQualifiedName()),
                astType.getSrcLoc());
        return;
    }

    if (!typeEnv.isType(astType.getBaseType())) {
        report.addError(tfm::format("Undefined base type %s in definition of type %s", astType.getBaseType(),
                                astType.getQualifiedName()),
                astType.getSrcLoc());
        return;
    }

    auto& rootType = typeEnv.getType(astType.getBaseType());

    if (isA<analysis::UnionType>(rootType)) {
        report.addError(tfm::format("Subset type %s can't be derived from union %s",
                                astType.getQualifiedName(), rootType.getName()),
                astType.getSrcLoc());
    }

    if (isA<analysis::RecordType>(rootType)) {
        report.addError(tfm::format("Subset type %s can't be derived from record type %s",
                                astType.getQualifiedName(), rootType.getName()),
                astType.getSrcLoc());
    }
}

void TypeDeclarationChecker::run() {
    // The redefinitions of types is checked by checkNamespaces in SemanticChecker
    for (auto* type : program.getTypes()) {
        if (typeEnv.isPrimitiveType(type->getQualifiedName())) {
            report.addError("Redefinition of the predefined type", type->getSrcLoc());
            continue;
        }

        if (isA<ast::UnionType>(type)) {
            checkUnionType(*as<ast::UnionType>(type));
        } else if (isA<ast::RecordType>(type)) {
            checkRecordType(*as<ast::RecordType>(type));
        } else if (isA<ast::SubsetType>(type)) {
            checkSubsetType(*as<ast::SubsetType>(type));
        } else if (isA<ast::AliasType>(type)) {
            checkAliasType(*as<ast::AliasType>(type));
        } else if (isA<ast::AlgebraicDataType>(type)) {
            checkADT(*as<ast::AlgebraicDataType>(type));
        } else {
            fatal("unsupported type construct: %s", typeid(type).name());
        }
    }

    // Check if all the branch names are unique in sum types.
    std::map<QualifiedName, std::vector<SrcLocation>> branchToLocation;
    visit(program.getTypes(), [&](const ast::AlgebraicDataType& type) {
        for (auto* branch : type.getBranches()) {
            branchToLocation[branch->getBranchName()].push_back(branch->getSrcLoc());
        }
    });

    for (auto& branchLocs : branchToLocation) {
        auto& branch = branchLocs.first;
        auto& locs = branchLocs.second;

        // If a branch is used only once, then everything is fine.
        if (locs.size() == 1) continue;

        auto primaryDiagnostic =
                DiagnosticMessage(tfm::format("Branch %s is defined multiple times", branch));

        std::vector<DiagnosticMessage> branchDeclarations;
        for (auto& loc : locs) {
            branchDeclarations.push_back(DiagnosticMessage(tfm::format("Branch %s defined", branch), loc));
        }

        report.addDiagnostic(Diagnostic(
                Diagnostic::Type::ERROR, std::move(primaryDiagnostic), std::move(branchDeclarations)));
    }
}

void TypeCheckerImpl::visit_(type_identity<Atom>, const Atom& atom) {
    auto relation = program.getRelation(atom);
    if (relation == nullptr) {
        return;  // error unrelated to types.
    }

    auto attributes = relation->getAttributes();
    auto arguments = atom.getArguments();
    if (attributes.size() != arguments.size()) {
        return;  // error in input program
    }

    for (std::size_t i = 0; i < attributes.size(); ++i) {
        auto& typeName = attributes[i]->getTypeName();
        if (!typeEnv.isType(typeName)) {
            continue;
        }

        auto argTypes = typeAnalysis.getTypes(arguments[i]);
        auto& attributeType = typeEnv.getType(typeName);

        if (argTypes.isAll() || argTypes.empty()) {
            continue;  // This will be reported later.
        }

        // We consider two cases: negated and not negated atoms.
        // Negated atom have to agree in kind, non-negated atom need to follow source/sink rules.
        if (negatedAtoms.count(&atom) == 0) {
            // Attribute and argument type agree if, argument type is a subtype of declared type
            // or is of the appropriate constant type or the (constant) record type.
            bool validAttribute = all_of(argTypes, [&attributeType](const analysis::Type& type) {
                if (isSubtypeOf(type, attributeType)) return true;
                if (!isSubtypeOf(attributeType, type)) return false;
                if (isA<ConstantType>(type)) return true;
                return isA<analysis::RecordType>(type) && !isA<analysis::SubsetType>(type);
            });

            if (!validAttribute && !Global::config().has("legacy")) {
                auto primaryDiagnostic =
                        DiagnosticMessage("Atom's argument type is not a subtype of its declared type",
                                arguments[i]->getSrcLoc());

                auto declaredTypeInfo =
                        DiagnosticMessage(tfm::format("The argument's declared type is %s", typeName),
                                attributes[i]->getSrcLoc());

                report.addDiagnostic(Diagnostic(Diagnostic::Type::ERROR, std::move(primaryDiagnostic),
                        {std::move(declaredTypeInfo)}));
            }
        } else {  // negation case.
            // Declared attribute and deduced type agree if:
            // They are the same type, or
            // They are derived from the same constant type.
            bool validAttribute = all_of(argTypes, [&](const analysis::Type& type) {
                return type == attributeType || any_of(typeEnv.getConstantTypes(), [&](auto& constantType) {
                    return isSubtypeOf(attributeType, constantType) && isSubtypeOf(type, constantType);
                });
            });

            if (!validAttribute) {
                auto primaryDiagnostic =
                        DiagnosticMessage("The kind of atom's argument doesn't match the declared type kind",
                                arguments[i]->getSrcLoc());
                auto declaredTypeInfo =
                        DiagnosticMessage(tfm::format("The argument's declared type is %s", typeName),
                                attributes[i]->getSrcLoc());
                report.addDiagnostic(Diagnostic(Diagnostic::Type::ERROR, std::move(primaryDiagnostic),
                        {std::move(declaredTypeInfo)}));
            }
        }
    }
}

void TypeCheckerImpl::visit_(type_identity<Variable>, const ast::Variable& var) {
    if (typeAnalysis.getTypes(&var).empty()) {
        report.addError("Unable to deduce type for variable " + var.getName(), var.getSrcLoc());
    }
}

void TypeCheckerImpl::visit_(type_identity<StringConstant>, const StringConstant& constant) {
    TypeSet types = typeAnalysis.getTypes(&constant);
    if (!isOfKind(types, TypeAttribute::Symbol)) {
        report.addError("Symbol constant (type mismatch)", constant.getSrcLoc());
    }
}

void TypeCheckerImpl::visit_(type_identity<NumericConstant>, const NumericConstant& constant) {
    TypeSet types = typeAnalysis.getTypes(&constant);

    // No type could be assigned.
    if (polyAnalysis.hasInvalidType(constant)) {
        report.addError("Ambiguous constant (unable to deduce type)", constant.getSrcLoc());
        return;
    }

    switch (polyAnalysis.getInferredType(constant)) {
        case NumericConstant::Type::Int:
            if (!isOfKind(types, TypeAttribute::Signed) || !canBeParsedAsRamSigned(constant.getConstant())) {
                report.addError("Number constant (type mismatch)", constant.getSrcLoc());
            }
            break;
        case NumericConstant::Type::Uint:
            if (!isOfKind(types, TypeAttribute::Unsigned) ||
                    !canBeParsedAsRamUnsigned(constant.getConstant())) {
                report.addError("Unsigned constant (type mismatch)", constant.getSrcLoc());
            }
            break;
        case NumericConstant::Type::Float:
            if (!isOfKind(types, TypeAttribute::Float) || !canBeParsedAsRamFloat(constant.getConstant())) {
                report.addError("Float constant (type mismatch)", constant.getSrcLoc());
            }
            break;
    }
}

void TypeCheckerImpl::visit_(type_identity<NilConstant>, const NilConstant& constant) {
    TypeSet types = typeAnalysis.getTypes(&constant);
    if (!isOfKind(types, TypeAttribute::Record)) {
        report.addError("Nil constant used as a non-record", constant.getSrcLoc());
        return;
    }
}

void TypeCheckerImpl::visit_(type_identity<RecordInit>, const RecordInit& rec) {
    TypeSet types = typeAnalysis.getTypes(&rec);

    if (!isOfKind(types, TypeAttribute::Record) || types.size() != 1) {
        report.addError("Ambiguous record", rec.getSrcLoc());
        return;
    }

    // At this point we know that there is exactly one type in set, so we can take it.
    auto& recordType = *as<analysis::RecordType>(getBaseType(&*types.begin()));

    if (recordType.getFields().size() != rec.getArguments().size()) {
        report.addError("Wrong number of arguments given to record", rec.getSrcLoc());
        return;
    }
}

void TypeCheckerImpl::visit_(type_identity<BranchInit>, const BranchInit& adt) {
    TypeSet types = typeAnalysis.getTypes(&adt);

    if (sumTypesBranches.getType(adt.getBranchName()) == nullptr) {
        report.addError("Undeclared branch", adt.getSrcLoc());
        return;
    }

    if (!isOfKind(types, TypeAttribute::ADT) || types.isAll() || types.size() != 1) {
        report.addError("Ambiguous branch", adt.getSrcLoc());
        return;
    }

    // We know now that the set "types" is a singleton
    auto& sumType = *as<analysis::AlgebraicDataType>(*types.begin());

    auto& argsDeclaredTypes = sumType.getBranchTypes(adt.getBranchName());

    auto args = adt.getArguments();

    if (argsDeclaredTypes.size() != args.size()) {
        report.addError(tfm::format("Invalid arity, the declared arity of %s is %s", adt.getBranchName(),
                                argsDeclaredTypes.size()),
                adt.getSrcLoc());
        return;
    }

    for (std::size_t i = 0; i < args.size(); ++i) {
        auto argTypes = typeAnalysis.getTypes(args[i]);
        bool correctType = all_of(
                argTypes, [&](const analysis::Type& t) { return isSubtypeOf(t, *argsDeclaredTypes[i]); });
        if (!correctType) {
            // TODO (darth_tytus): Give better error
            report.addError("Branch argument's type doesn't match its declared type", args[i]->getSrcLoc());
        }
    }
}

void TypeCheckerImpl::visit_(type_identity<TypeCast>, const ast::TypeCast& cast) {
    if (!typeEnv.isType(cast.getType())) {
        report.addError(
                tfm::format("Type cast to the undeclared type \"%s\"", cast.getType()), cast.getSrcLoc());
        return;
    }

    auto& castTypes = typeAnalysis.getTypes(&cast);
    auto& argTypes = typeAnalysis.getTypes(cast.getValue());

    if (castTypes.isAll() || castTypes.size() != 1) {
        report.addError("Unable to deduce type of the argument (cast)", cast.getSrcLoc());
        return;
    }

    // This should be reported elsewhere
    if (argTypes.isAll() || castTypes.size() != 1 || argTypes.isAll() || argTypes.size() != 1) {
        return;
    }
}

void TypeCheckerImpl::visit_(type_identity<IntrinsicFunctor>, const IntrinsicFunctor& fun) {
    if (!typeAnalysis.hasValidTypeInfo(fun)) {
        auto args = fun.getArguments();
        if (!isValidFunctorOpArity(fun.getBaseFunctionOp(), args.size())) {
            report.addError("invalid overload (arity mismatch)", fun.getSrcLoc());
            return;
        }
        assert(typeAnalysis.getValidIntrinsicFunctorOverloads(fun).empty() &&
                "unexpected type analysis result");
        report.addError("no valid overloads", fun.getSrcLoc());
    }
}

void TypeCheckerImpl::visit_(type_identity<UserDefinedFunctor>, const UserDefinedFunctor& fun) {
    // check type of result
    const TypeSet& resultTypes = typeAnalysis.getTypes(&fun);

    using Type = analysis::Type;

    auto const* udfd = getFunctorDeclaration(program, fun.getName());
    if (udfd == nullptr || !typeAnalysis.hasValidTypeInfo(*udfd)) {
        // I found this very confusing, hopefully this comment helps someone else.
        // I assumed that this branch cannot be taken, because the Semantic checker
        // verifies that every functor has a declaration!  However, it turns out that
        // the SemanticChecker is *not* the first Transformer which gets run and so
        // it's not really clear that those invariants hold yet.
        // I don't particularly like that but am not at a place where I can change the
        // order of the passes/transformers.  So, for now, here's a comment for the next
        // person going doing this rabbit hole.
        return;
    }

    Type const& returnType = typeAnalysis.getFunctorReturnType(fun);

    if (resultTypes.isAll() || resultTypes.size() != 1) {
        std::ostringstream out;
        out << "Invalid use of functor returning " << returnType;
        report.addError(out.str(), fun.getSrcLoc());
    } else {
        Type const& resultType = *resultTypes.begin();

        if (!isSubtypeOf(returnType, resultType)) {
            std::ostringstream out;
            out << "Invalid conversion of return type " << returnType << " to " << resultType;
            report.addError(out.str(), fun.getSrcLoc());
        }
    }

    auto const& params = udfd->getParams();
    auto const arity = params.size();
    auto const& args = fun.getArguments();
    auto const toCheck = std::min(args.size(), arity);
    if (args.size() != arity) {
        std::ostringstream out;
        out << "Functor arity mismatch: Got " << args.size() << " arguments, expecting " << arity;
        report.addError(out.str(), fun.getSrcLoc());
    }

    auto getName = [&params](std::size_t idx) {
        auto const& name = params[idx]->getName();
        std::ostringstream out;
        out << "positional parameter " << idx;
        if (!name.empty()) {
            out << " ('" << name << "')";
        }

        return out.str();
    };

    for (std::size_t ii = 0; ii < toCheck; ++ii) {
        auto const& arg = args[ii];
        Type const& paramType = typeAnalysis.getFunctorParamType(fun, ii);
        TypeSet const& argTypes = typeAnalysis.getTypes(arg);

        if (argTypes.isAll() || argTypes.size() != 1) {
            report.addError("Unable to determine type for " + getName(ii), arg->getSrcLoc());
        } else {
            Type const& argType = *argTypes.begin();

            if (!isSubtypeOf(argType, paramType)) {
                std::ostringstream out;
                out << "Invalid conversion of value of type " << argType << " to " << getName(ii)
                    << " with type " << paramType;
                report.addError(out.str(), arg->getSrcLoc());
            }
        }
    }
}

void TypeCheckerImpl::visit_(type_identity<BinaryConstraint>, const BinaryConstraint& constraint) {
    auto op = polyAnalysis.getOverloadedOperator(constraint);
    auto left = constraint.getLHS();
    auto right = constraint.getRHS();
    auto opTypesAttrs = getBinaryConstraintTypes(op);

    auto leftTypes = typeAnalysis.getTypes(left);
    auto rightTypes = typeAnalysis.getTypes(right);

    // Skip checks if either side could not be fully deduced
    // The unable-to-deduce-type checker will point out the issue.
    if (leftTypes.isAll() || leftTypes.size() != 1) return;
    if (rightTypes.isAll() || rightTypes.size() != 1) return;

    // Extract types from singleton sets.
    auto& leftType = *typeAnalysis.getTypes(left).begin();
    auto& rightType = *typeAnalysis.getTypes(right).begin();

    // give them a slightly nicer error
    if (isOrderedBinaryConstraintOp(op) && !areEquivalentTypes(leftType, rightType)) {
        report.addError("Cannot compare different types", constraint.getSrcLoc());
    } else {
        auto checkTyAttr = [&](Argument const& side) {
            auto opMatchesType = any_of(opTypesAttrs,
                    [&](auto& typeAttr) { return isOfKind(typeAnalysis.getTypes(&side), typeAttr); });

            if (!opMatchesType) {
                std::stringstream ss;
                ss << "Constraint requires an operand of type "
                   << join(opTypesAttrs, " or ", [&](auto& out, auto& typeAttr) {
                          switch (typeAttr) {
                              case TypeAttribute::Signed: out << "`number`"; break;
                              case TypeAttribute::Symbol: out << "`symbol`"; break;
                              case TypeAttribute::Unsigned: out << "`unsigned`"; break;
                              case TypeAttribute::Float: out << "`float`"; break;
                              case TypeAttribute::Record: out << "a record"; break;
                              case TypeAttribute::ADT: out << "a sum"; break;
                          }
                      });
                report.addError(ss.str(), side.getSrcLoc());
            }
        };

        checkTyAttr(*left);
        checkTyAttr(*right);
    }
}

void TypeCheckerImpl::visit_(type_identity<Aggregator>, const Aggregator& aggregator) {
    auto op = polyAnalysis.getOverloadedOperator(aggregator);

    auto aggregatorType = typeAnalysis.getTypes(&aggregator);

    TypeAttribute opType = getTypeAttributeAggregate(op);

    // Check if operation type and return type agree.
    if (!isOfKind(aggregatorType, opType)) {
        report.addError("Couldn't assign types to the aggregator", aggregator.getSrcLoc());
    }
}

void TypeCheckerImpl::visit_(type_identity<Negation>, const Negation& neg) {
    negatedAtoms.insert(neg.getAtom());
}

}  // namespace souffle::ast::transform
