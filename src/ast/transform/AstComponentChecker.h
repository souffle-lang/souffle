/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstComponentChecker.h
 *
 * Defines the component semantic checker pass.
 *
 ***********************************************************************/

#pragma once

#include "ast/transform/AstTransformer.h"
#include <string>

namespace souffle {

class AstComponent;
class AstComponentType;
class AstComponentInit;
class AstProgram;
class SrcLocation;
class AstTranslationUnit;
class ComponentLookup;
class ErrorReport;
class TypeBinding;

class AstComponentChecker : public AstTransformer {
public:
    ~AstComponentChecker() override = default;

    std::string getName() const override {
        return "AstComponentChecker";
    }

    AstComponentChecker* clone() const override {
        return new AstComponentChecker();
    }

private:
    bool transform(AstTranslationUnit& translationUnit) override;

    static const AstComponent* checkComponentNameReference(ErrorReport& report,
            const AstComponent* enclosingComponent, const ComponentLookup& componentLookup,
            const std::string& name, const SrcLocation& loc, const TypeBinding& binding);
    static void checkComponentReference(ErrorReport& report, const AstComponent* enclosingComponent,
            const ComponentLookup& componentLookup, const AstComponentType& type, const SrcLocation& loc,
            const TypeBinding& binding);
    static void checkComponentInit(ErrorReport& report, const AstComponent* enclosingComponent,
            const ComponentLookup& componentLookup, const AstComponentInit& init, const TypeBinding& binding);
    static void checkComponent(ErrorReport& report, const AstComponent* enclosingComponent,
            const ComponentLookup& componentLookup, const AstComponent& component,
            const TypeBinding& binding);
    static void checkComponents(
            ErrorReport& report, const AstProgram& program, const ComponentLookup& componentLookup);
    static void checkComponentNamespaces(ErrorReport& report, const AstProgram& program);
};

}  // namespace souffle
