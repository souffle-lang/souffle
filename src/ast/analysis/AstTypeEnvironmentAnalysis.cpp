/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypeEnvironmentAnalysis.cpp
 *
 * Implements AST Analysis methods for a Type Environment
 *
 ***********************************************************************/

#include "ast/analysis/AstTypeEnvironmentAnalysis.h"
#include "GraphUtils.h"
#include "ast/AstAttribute.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstType.h"
#include "ast/TypeSystem.h"
#include "utility/MiscUtil.h"
#include "utility/tinyformat.h"
#include <functional>
#include <ostream>
#include <typeinfo>
#include <utility>
#include <vector>

namespace souffle {

namespace {

Graph<AstQualifiedName> createTypeDependencyGraph(const std::vector<AstType*>& programTypes) {
    Graph<AstQualifiedName> typeDependencyGraph;
    for (const auto* astType : programTypes) {
        if (auto type = as<AstSubsetType>(astType)) {
            typeDependencyGraph.insert(type->getQualifiedName(), type->getBaseType());
        } else if (isA<AstRecordType>(astType)) {
            // do nothing
        } else if (auto type = as<AstUnionType>(astType)) {
            for (const auto& subtype : type->getTypes()) {
                typeDependencyGraph.insert(type->getQualifiedName(), subtype);
            }
        } else {
            fatal("unsupported type construct: %s", typeid(astType).name());
        }
    }
    return typeDependencyGraph;
}

/**
 * Find all the type with a cyclic definition (in terms of being a subtype)
 */
std::set<AstQualifiedName> analyseCyclicTypes(
        const Graph<AstQualifiedName>& dependencyGraph, const std::vector<AstType*>& programTypes) {
    std::set<AstQualifiedName> cyclicTypes;
    for (const auto& astType : programTypes) {
        AstQualifiedName typeName = astType->getQualifiedName();

        if (dependencyGraph.reaches(typeName, typeName)) {
            cyclicTypes.insert(std::move(typeName));
        }
    }
    return cyclicTypes;
}

/**
 * Find all the primitive types that are the subtypes of the union types.
 */
std::map<AstQualifiedName, std::set<AstQualifiedName>> analysePrimitiveTypesInUnion(
        const Graph<AstQualifiedName>& dependencyGraph, const std::vector<AstType*>& programTypes,
        const TypeEnvironment& env) {
    std::map<AstQualifiedName, std::set<AstQualifiedName>> primitiveTypesInUnions;

    for (const auto& astType : programTypes) {
        auto* unionType = as<AstUnionType>(astType);
        if (unionType == nullptr) {
            continue;
        }
        AstQualifiedName unionName = unionType->getQualifiedName();

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

void TypeEnvironmentAnalysis::run(const AstTranslationUnit& translationUnit) {
    const AstProgram& program = *translationUnit.getProgram();

    auto rawProgramTypes = program.getTypes();
    Graph<AstQualifiedName> typeDependencyGraph{createTypeDependencyGraph(rawProgramTypes)};

    cyclicTypes = analyseCyclicTypes(typeDependencyGraph, rawProgramTypes);
    primitiveTypesInUnions = analysePrimitiveTypesInUnion(typeDependencyGraph, rawProgramTypes, env);

    std::map<AstQualifiedName, const AstType*> nameToAstType;

    // Filter redefined primitive types and cyclic types.
    std::vector<AstType*> programTypes;
    for (auto* type : program.getTypes()) {
        if (env.isType(type->getQualifiedName()) || isCyclic(type->getQualifiedName())) {
            continue;
        }
        programTypes.push_back(type);
        nameToAstType.insert({type->getQualifiedName(), type});
    }

    for (const auto* astType : programTypes) {
        createType(astType->getQualifiedName(), nameToAstType);
    }
}

const Type* TypeEnvironmentAnalysis::createType(
        const AstQualifiedName& typeName, const std::map<AstQualifiedName, const AstType*>& nameToAstType) {
    // base case
    if (env.isType(typeName)) {
        return &env.getType(typeName);
    }

    // Handle undeclared type in the definition of another type.
    auto iterToType = nameToAstType.find(typeName);
    if (iterToType == nameToAstType.end()) {
        return nullptr;
    }
    const auto& astType = iterToType->second;

    if (isA<AstSubsetType>(astType)) {
        // First create a base type.
        auto* baseType = createType(as<AstSubsetType>(astType)->getBaseType(), nameToAstType);

        if (baseType == nullptr) {
            return nullptr;
        }

        // Subset of a record is a special case.
        if (isA<RecordType>(baseType)) {
            return &env.createType<SubsetRecordType>(typeName, *as<RecordType>(baseType));
        }

        return &env.createType<SubsetType>(typeName, *baseType);

    } else if (isA<AstUnionType>(astType)) {
        // Create all elements and then the type itself
        std::vector<const Type*> elements;
        for (const auto& element : as<AstUnionType>(astType)->getTypes()) {
            auto* elementType = createType(element, nameToAstType);
            if (elementType == nullptr) {
                return nullptr;
            }
            elements.push_back(elementType);
        }
        return &env.createType<UnionType>(typeName, elements);

    } else if (isA<AstRecordType>(astType)) {
        // Create anonymous base type first.
        // The types need to be initialized upfront,
        // because we may have mutually recursive record definitions.
        auto& recordBase = env.createType<RecordType>(tfm::format("__%sConstant", typeName));
        auto& recordType = env.createType<SubsetRecordType>(typeName, recordBase);

        std::vector<const Type*> elements;
        for (const auto* field : as<AstRecordType>(astType)->getFields()) {
            if (field->getTypeName() == typeName) {
                elements.push_back(&recordBase);
                continue;
            }

            auto* elementType = createType(field->getTypeName(), nameToAstType);
            if (elementType == nullptr) {
                return nullptr;
            }
            elements.push_back(elementType);
        }

        recordBase.setFields(elements);
        std::replace(elements.begin(), elements.end(), dynamic_cast<Type*>(&recordBase),
                dynamic_cast<Type*>(&recordType));
        recordType.setFields(std::move(elements));

        return &recordType;

    } else {
        fatal("unsupported type construct: %s", typeid(*astType).name());
    }
}

void TypeEnvironmentAnalysis::print(std::ostream& os) const {
    env.print(os);
}

}  // namespace souffle
