/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentLookupAnalysis.cpp
 *
 * Implements the component lookup
 *
 ***********************************************************************/

#include "ComponentLookupAnalysis.h"
#include "ast/AstComponent.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstVisitor.h"
#include "utility/StringUtil.h"

namespace souffle {

void ComponentLookup::run(const AstTranslationUnit& translationUnit) {
    const AstProgram* program = translationUnit.getProgram();
    for (AstComponent* component : program->getComponents()) {
        globalScopeComponents.insert(component);
        enclosingComponent[component] = nullptr;
    }
    visitDepthFirst(*program, [&](const AstComponent& cur) {
        nestedComponents[&cur];
        for (AstComponent* nestedComponent : cur.getComponents()) {
            nestedComponents[&cur].insert(nestedComponent);
            enclosingComponent[nestedComponent] = &cur;
        }
    });
}

const AstComponent* ComponentLookup::getComponent(
        const AstComponent* scope, const std::string& name, const TypeBinding& activeBinding) const {
    // forward according to binding (we do not do this recursively on purpose)
    AstQualifiedName boundName = activeBinding.find(name);
    if (boundName.empty()) {
        // compName is not bound to anything => just just compName
        boundName = name;
    }

    // search nested scopes bottom up
    const AstComponent* searchScope = scope;
    while (searchScope != nullptr) {
        for (const AstComponent* cur : searchScope->getComponents()) {
            if (cur->getComponentType()->getName() == toString(boundName)) {
                return cur;
            }
        }
        auto found = enclosingComponent.find(searchScope);
        if (found != enclosingComponent.end()) {
            searchScope = found->second;
        } else {
            searchScope = nullptr;
            break;
        }
    }

    // check global scope
    for (const AstComponent* cur : globalScopeComponents) {
        if (cur->getComponentType()->getName() == toString(boundName)) {
            return cur;
        }
    }

    // no such component in scope
    return nullptr;
}

}  // end namespace souffle
