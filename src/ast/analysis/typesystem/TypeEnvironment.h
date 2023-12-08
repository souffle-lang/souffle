/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TypeEnvironment.h
 *
 * A wrapper for TypeEnvironment to be used for AST Analysis
 *
 ***********************************************************************/

#pragma once

#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "ast/analysis/typesystem/TypeSystem.h"
#include "souffle/utility/ContainerUtil.h"
#include <iosfwd>
#include <map>
#include <set>
#include <string>

namespace souffle::ast::analysis {

class TypeEnvironmentAnalysis : public Analysis {
public:
    static constexpr const char* name = "type-environment";

    TypeEnvironmentAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    void print(std::ostream& os) const override;

    const TypeEnvironment& getTypeEnvironment() const {
        return env;
    }

    const UnorderedQualifiedNameSet& getPrimitiveTypesInUnion(const QualifiedName& identifier) const {
        return primitiveTypesInUnions.at(identifier);
    }

    bool isCyclic(const QualifiedName& identifier) const {
        return contains(cyclicTypes, identifier);
    }

private:
    TypeEnvironment env;
    UnorderedQualifiedNameMap<UnorderedQualifiedNameSet> primitiveTypesInUnions;
    UnorderedQualifiedNameSet cyclicTypes;

    /**
     * Recursively create a type in env, that is
     * first create its base types and then the type itself.
     */
    const Type* createType(
            const QualifiedName& typeName, const UnorderedQualifiedNameMap<const ast::Type*>& nameToType);
};

}  // namespace souffle::ast::analysis
