/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstComponentChecker.cpp
 *
 * Implementation of the component semantic checker pass.
 *
 ***********************************************************************/

#include "ast/transform/AstComponentChecker.h"
#include "ErrorReport.h"
#include "RelationTag.h"
#include "SrcLocation.h"
#include "ast/AstComponent.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstType.h"
#include "ast/analysis/ComponentLookupAnalysis.h"
#include "utility/StringUtil.h"
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

bool AstComponentChecker::transform(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();
    ComponentLookup& componentLookup = *translationUnit.getAnalysis<ComponentLookup>();
    ErrorReport& report = translationUnit.getErrorReport();
    checkComponents(report, program, componentLookup);
    checkComponentNamespaces(report, program);
    return false;
}

const AstComponent* AstComponentChecker::checkComponentNameReference(ErrorReport& report,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup,
        const std::string& name, const SrcLocation& loc, const TypeBinding& binding) {
    const AstQualifiedName& forwarded = binding.find(name);
    if (!forwarded.empty()) {
        // for forwarded types we do not check anything, because we do not know
        // what the actual type will be
        return nullptr;
    }

    const AstComponent* c = componentLookup.getComponent(enclosingComponent, name, binding);
    if (c == nullptr) {
        report.addError("Referencing undefined component " + name, loc);
        return nullptr;
    }

    return c;
}

void AstComponentChecker::checkComponentReference(ErrorReport& report, const AstComponent* enclosingComponent,
        const ComponentLookup& componentLookup, const AstComponentType& type, const SrcLocation& loc,
        const TypeBinding& binding) {
    // check whether targeted component exists
    const AstComponent* c = checkComponentNameReference(
            report, enclosingComponent, componentLookup, type.getName(), loc, binding);
    if (c == nullptr) {
        return;
    }

    // check number of type parameters
    if (c->getComponentType()->getTypeParameters().size() != type.getTypeParameters().size()) {
        report.addError("Invalid number of type parameters for component " + type.getName(), loc);
    }
}

void AstComponentChecker::checkComponentInit(ErrorReport& report, const AstComponent* enclosingComponent,
        const ComponentLookup& componentLookup, const AstComponentInit& init, const TypeBinding& binding) {
    checkComponentReference(
            report, enclosingComponent, componentLookup, *init.getComponentType(), init.getSrcLoc(), binding);

    // Note: actual parameters can be atomic types like number, or anything declared with .type
    // The original semantic check permitted any identifier (existing or non-existing) to be actual parameter
    // In order to maintain the compatibility, we do not check the actual parameters

    // check the actual parameters:
    // const auto& actualParams = init.getComponentType().getTypeParameters();
    // for (const auto& param : actualParams) {
    //    checkComponentNameReference(report, scope, param, init.getSrcLoc(), binding);
    //}
}

void AstComponentChecker::checkComponent(ErrorReport& report, const AstComponent* enclosingComponent,
        const ComponentLookup& componentLookup, const AstComponent& component, const TypeBinding& binding) {
    // -- inheritance --

    // Update type binding:
    // Since we are not compiling, i.e. creating concrete instance of the
    // components with type parameters, we are only interested in whether
    // component references refer to existing components or some type parameter.
    // Type parameter for us here is unknown type that will be bound at the template
    // instantiation time.
    auto parentTypeParameters = component.getComponentType()->getTypeParameters();
    std::vector<AstQualifiedName> actualParams(parentTypeParameters.size(), "<type parameter>");
    TypeBinding activeBinding = binding.extend(parentTypeParameters, actualParams);

    // check parents of component
    for (const auto& cur : component.getBaseComponents()) {
        checkComponentReference(
                report, enclosingComponent, componentLookup, *cur, component.getSrcLoc(), activeBinding);

        // Note: type parameters can also be atomic types like number, or anything defined through .type
        // The original semantic check permitted any identifier (existing or non-existing) to be actual
        // parameter
        // In order to maintain the compatibility, we do not check the actual parameters

        // for (const std::string& param : cur.getTypeParameters()) {
        //    checkComponentNameReference(report, scope, param, component.getSrcLoc(), activeBinding);
        //}
    }

    // get all parents
    std::set<const AstComponent*> parents;
    std::function<void(const AstComponent&)> collectParents = [&](const AstComponent& cur) {
        for (const auto& base : cur.getBaseComponents()) {
            auto c = componentLookup.getComponent(enclosingComponent, base->getName(), binding);
            if (c == nullptr) {
                continue;
            }
            if (parents.insert(c).second) {
                collectParents(*c);
            }
        }
    };
    collectParents(component);

    // check overrides
    for (const AstRelation* relation : component.getRelations()) {
        if (component.getOverridden().count(relation->getQualifiedName().getQualifiers()[0]) != 0u) {
            report.addError("Override of non-inherited relation " +
                                    relation->getQualifiedName().getQualifiers()[0] + " in component " +
                                    component.getComponentType()->getName(),
                    component.getSrcLoc());
        }
    }
    for (const AstComponent* parent : parents) {
        for (const AstRelation* relation : parent->getRelations()) {
            if ((component.getOverridden().count(relation->getQualifiedName().getQualifiers()[0]) != 0u) &&
                    !relation->hasQualifier(RelationQualifier::OVERRIDABLE)) {
                report.addError("Override of non-overridable relation " +
                                        relation->getQualifiedName().getQualifiers()[0] + " in component " +
                                        component.getComponentType()->getName(),
                        component.getSrcLoc());
            }
        }
    }

    // check for a cycle
    if (parents.find(&component) != parents.end()) {
        report.addError(
                "Invalid cycle in inheritance for component " + component.getComponentType()->getName(),
                component.getSrcLoc());
    }

    // -- nested components --

    // check nested components
    for (const auto& cur : component.getComponents()) {
        checkComponent(report, &component, componentLookup, *cur, activeBinding);
    }

    // check nested instantiations
    for (const auto& cur : component.getInstantiations()) {
        checkComponentInit(report, &component, componentLookup, *cur, activeBinding);
    }
}

void AstComponentChecker::checkComponents(
        ErrorReport& report, const AstProgram& program, const ComponentLookup& componentLookup) {
    for (AstComponent* cur : program.getComponents()) {
        checkComponent(report, nullptr, componentLookup, *cur, TypeBinding());
    }

    for (AstComponentInit* cur : program.getComponentInstantiations()) {
        checkComponentInit(report, nullptr, componentLookup, *cur, TypeBinding());
    }
}

// Check that component names are disjoint from type and relation names.
void AstComponentChecker::checkComponentNamespaces(ErrorReport& report, const AstProgram& program) {
    std::map<std::string, SrcLocation> names;

    // Type and relation name error reporting performed by the AstSemanticChecker instead

    // Find all names and report redeclarations as we go.
    for (const auto& type : program.getTypes()) {
        const std::string name = toString(type->getQualifiedName());
        if (names.count(name) == 0u) {
            names[name] = type->getSrcLoc();
        }
    }

    for (const auto& rel : program.getRelations()) {
        const std::string name = toString(rel->getQualifiedName());
        if (names.count(name) == 0u) {
            names[name] = rel->getSrcLoc();
        }
    }

    // Note: Nested component and instance names are not obtained.
    for (const auto& comp : program.getComponents()) {
        const std::string name = toString(comp->getComponentType()->getName());
        if (names.count(name) != 0u) {
            report.addError("Name clash on component " + name, comp->getSrcLoc());
        } else {
            names[name] = comp->getSrcLoc();
        }
    }

    for (const auto& inst : program.getComponentInstantiations()) {
        const std::string name = toString(inst->getInstanceName());
        if (names.count(name) != 0u) {
            report.addError("Name clash on instantiation " + name, inst->getSrcLoc());
        } else {
            names[name] = inst->getSrcLoc();
        }
    }
}
}  // namespace souffle
