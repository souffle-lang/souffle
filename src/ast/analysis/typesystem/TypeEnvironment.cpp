/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeEnvironment.cpp
 *
 * Implements AST Analysis methods for a Type Environment
 *
 ***********************************************************************/

#include "ast/analysis/typesystem/TypeEnvironment.h"
#include "GraphUtils.h"
#include "ast/AlgebraicDataType.h"
#include "ast/AliasType.h"
#include "ast/Attribute.h"
#include "ast/BranchType.h"
#include "ast/Program.h"
#include "ast/RecordType.h"
#include "ast/SubsetType.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "ast/UnionType.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/tinyformat.h"
#include <algorithm>
#include <functional>
#include <ostream>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

namespace {

using QNGraph = GraphLabeled<QualifiedName, Unit, UnorderedQualifiedNameLess>;

QNGraph createTypeDependencyGraph(const std::vector<ast::Type*>& programTypes) {
    QNGraph typeDependencyGraph;
    for (const auto* astType : programTypes) {
        if (auto type = as<ast::SubsetType>(astType)) {
            typeDependencyGraph.insert(type->getQualifiedName(), type->getBaseType());
        } else if (auto type = as<ast::AliasType>(astType)) {
            typeDependencyGraph.insert(type->getQualifiedName(), type->getAliasType());
        } else if (auto type = as<ast::UnionType>(astType)) {
            for (const auto& subtype : type->getTypes()) {
                typeDependencyGraph.insert(type->getQualifiedName(), subtype);
            }
        } else if (isA<ast::RecordType>(astType)) {
#if 0
            // we should add that so that we have the complete type dependency graph
            for (const auto& field : type->getFields() ) {
                typeDependencyGraph.insert(type->getQualifiedName(), field->getAttributeType());
            }
#endif
        } else if (isA<ast::AlgebraicDataType>(astType)) {
#if 0
            // we should add that so that we have the complete type dependency graph
            for (const auto& branch: type->getBranches()) { 
              for (const auto& field : branch->getFields() ) {
                typeDependencyGraph.insert(type->getQualifiedName(), field->getAttributeType());
              }
            }
#endif
        } else {
            fatal("unsupported type construct: %s", typeid(astType).name());
        }
    }
    return typeDependencyGraph;
}

/**
 * Find all the type with a cyclic definition (in terms of being a subtype/alias)
 */
UnorderedQualifiedNameSet analyseCyclicTypes(
        const QNGraph& dependencyGraph, const std::vector<ast::Type*>& programTypes) {
    UnorderedQualifiedNameSet cyclicTypes;
    for (const auto& astType : programTypes) {
        QualifiedName typeName = astType->getQualifiedName();
        if (dependencyGraph.reaches(typeName, typeName)) {
            cyclicTypes.insert(std::move(typeName));
        }
    }
    return cyclicTypes;
}

/**
 * Find all the primitive types that are the subtypes of the union types.
 */
UnorderedQualifiedNameMap<UnorderedQualifiedNameSet> analysePrimitiveTypesInUnion(
        const QNGraph& dependencyGraph, const std::vector<ast::Type*>& programTypes,
        const TypeEnvironment& env) {
    UnorderedQualifiedNameMap<UnorderedQualifiedNameSet> primitiveTypesInUnions;

    for (const auto& astType : programTypes) {
        auto* unionType = as<ast::UnionType>(astType);
        if (unionType == nullptr) {
            continue;
        }
        QualifiedName unionName = unionType->getQualifiedName();

        auto iteratorToUnion = primitiveTypesInUnions.find(unionName);

        // Initialize with the empty set
        if (iteratorToUnion == primitiveTypesInUnions.end()) {
            iteratorToUnion = primitiveTypesInUnions.insert({unionName, {}}).first;
        }

        auto& associatedTypes = iteratorToUnion->second;

        // Insert any reachable primitive type
        for (auto& type : env.getPrimitiveTypes()) {
            if (dependencyGraph.reaches(unionName, type.getName())) {
                associatedTypes.insert(type.getName());
            }
        }
    }
    return primitiveTypesInUnions;
}

}  // namespace

void TypeEnvironmentAnalysis::run(const TranslationUnit& translationUnit) {
    const Program& program = translationUnit.getProgram();

    auto rawProgramTypes = program.getTypes();
    QNGraph typeDependencyGraph{createTypeDependencyGraph(rawProgramTypes)};

    cyclicTypes = analyseCyclicTypes(typeDependencyGraph, rawProgramTypes);

    primitiveTypesInUnions = analysePrimitiveTypesInUnion(typeDependencyGraph, rawProgramTypes, env);

    UnorderedQualifiedNameMap<const ast::Type*> nameToType;

    // Filter redefined primitive types and cyclic types.
    std::vector<ast::Type*> programTypes;
    for (auto* type : program.getTypes()) {
        if (env.isType(type->getQualifiedName()) || isCyclic(type->getQualifiedName())) {
            continue;
        }
        programTypes.push_back(type);
        nameToType.insert({type->getQualifiedName(), type});
    }

    for (const auto* astType : programTypes) {
        createType(astType->getQualifiedName(), nameToType);
    }
}

const Type* TypeEnvironmentAnalysis::createType(
        const QualifiedName& typeName, const UnorderedQualifiedNameMap<const ast::Type*>& nameToType) {
    // base case
    if (env.isType(typeName)) {
        return &env.getType(typeName);
    }

    // Handle undeclared type in the definition of another type.
    auto iterToType = nameToType.find(typeName);
    if (iterToType == nameToType.end()) {
        return nullptr;
    }
    const auto& astType = iterToType->second;

    if (isA<ast::SubsetType>(astType)) {
        // First create a base type
        auto* baseType = createType(as<ast::SubsetType>(astType)->getBaseType(), nameToType);

        if (baseType == nullptr) {
            return nullptr;
        }

        return &env.createType<SubsetType>(typeName, *baseType);

    } else if (isA<ast::AliasType>(astType)) {
        // First create an alias type
        auto* aliasType = createType(as<ast::AliasType>(astType)->getAliasType(), nameToType);

        if (aliasType == nullptr) {
            return nullptr;
        }

        return &env.createType<AliasType>(typeName, *aliasType);

    } else if (isA<ast::UnionType>(astType)) {
        // Create all elements and then the type itself
        std::vector<const Type*> elements;
        for (const auto& element : as<ast::UnionType>(astType)->getTypes()) {
            auto* elementType = createType(element, nameToType);
            if (elementType == nullptr) {
                return nullptr;
            }
            elements.push_back(elementType);
        }
        return &env.createType<UnionType>(typeName, elements);

    } else if (auto astRecordType = as<ast::RecordType>(astType)) {
        // Create the corresponding type first, since it could be recursive.
        auto& recordType = env.createType<RecordType>(typeName);

        std::vector<const Type*> elements;
        for (const auto* field : astRecordType->getFields()) {
            if (field->getTypeName() == typeName) {
                elements.push_back(&recordType);
                continue;
            }

            // Recursively create element's type.
            auto* elementType = createType(field->getTypeName(), nameToType);
            if (elementType == nullptr) {
                return nullptr;
            }
            elements.push_back(elementType);
        }

        recordType.setFields(std::move(elements));
        return &recordType;

    } else if (isA<ast::AlgebraicDataType>(astType)) {
        // ADT can be recursive so its need to be forward initialized
        auto& adt = env.createType<AlgebraicDataType>(typeName);

        std::vector<AlgebraicDataType::Branch> elements;

        // Create and collect branches types.
        for (auto* branch : as<ast::AlgebraicDataType>(astType)->getBranches()) {
            std::vector<const Type*> branchTypes;

            for (auto* attr : branch->getFields()) {
                auto* type = createType(attr->getTypeName(), nameToType);
                if (type == nullptr) return nullptr;
                branchTypes.push_back(type);
            }
            elements.push_back({branch->getBranchName(), std::move(branchTypes)});
        }

        adt.setBranches(std::move(elements));

        return &adt;
    } else {
        fatal("unsupported type construct: %s", typeid(*astType).name());
    }
}

void TypeEnvironmentAnalysis::print(std::ostream& os) const {
    env.print(os);
}

}  // namespace souffle::ast::analysis
