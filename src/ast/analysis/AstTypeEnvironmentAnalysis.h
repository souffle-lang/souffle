/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypeEnvironmentAnalysis.h
 *
 * A wrapper for TypeEnvironment to be used for AST Analysis
 *
 ***********************************************************************/

#pragma once

#include "GraphUtils.h"
#include "RamTypes.h"
#include "ast/AstQualifiedName.h"
#include "ast/TypeSystem.h"
#include "ast/analysis/AstAnalysis.h"
#include "utility/ContainerUtil.h"
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace souffle {

class AstTranslationUnit;
class AstType;

class TypeEnvironmentAnalysis : public AstAnalysis {
public:
    static constexpr const char* name = "type-environment";

    TypeEnvironmentAnalysis() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const TypeEnvironment& getTypeEnvironment() const {
        return env;
    }

    const std::set<AstQualifiedName>& getPrimitiveTypesInUnion(const AstQualifiedName& identifier) const {
        return primitiveTypesInUnions.at(identifier);
    }

    bool isCyclic(const AstQualifiedName& identifier) const {
        return contains(cyclicTypes, identifier);
    }

private:
    TypeEnvironment env;
    std::map<AstQualifiedName, std::set<AstQualifiedName>> primitiveTypesInUnions;
    std::set<AstQualifiedName> cyclicTypes;

    /**
     * Recursively create a type in env, that is
     * first create its base types and then the type itself.
     */
    const Type* createType(const AstQualifiedName& typeName,
            const std::map<AstQualifiedName, const AstType*>& nameToAstType);
};

}  // end of namespace souffle
