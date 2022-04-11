/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
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

#include "ast/analysis/ComponentLookup.h"
#include "ast/Component.h"
#include "ast/ComponentType.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/StringUtil.h"

namespace souffle::ast::analysis {

void ComponentLookupAnalysis::run(const TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    for (Component* component : program.getComponents()) {
        globalScopeComponents.insert(component);
        enclosingComponent[component] = nullptr;
    }
    visit(program, [&](const Component& cur) {
        nestedComponents[&cur];
        for (Component* nestedComponent : cur.getComponents()) {
            nestedComponents[&cur].insert(nestedComponent);
            enclosingComponent[nestedComponent] = &cur;
        }
    });
}

const Component* ComponentLookupAnalysis::getComponent(
        const Component* scope, const std::string& name, const TypeBinding& activeBinding) const {
    // forward according to binding (we do not do this recursively on purpose)
    QualifiedName boundName = activeBinding.find(name);
    if (boundName.empty()) {
        // compName is not bound to anything => just just compName
        boundName = name;
    }

    // search nested scopes bottom up
    const Component* searchScope = scope;
    while (searchScope != nullptr) {
        // search in components declared in scope
        for (const Component* cur : searchScope->getComponents()) {
            if (cur->getComponentType()->getName() == toString(boundName)) {
                return cur;
            }
        }

        // also search in bases
        for (const auto& baseType : searchScope->getBaseComponents()) {
            const auto found = enclosingComponent.find(searchScope);
            const Component* const scopeEnclosingComponent =
                    (found == enclosingComponent.end() ? nullptr : found->second);
            // search base component
            const Component* const base =
                    getComponent(scopeEnclosingComponent, baseType->getName(), activeBinding);
            if (base == searchScope) {
                return nullptr;
            }
            if (base != nullptr) {
                const Component* const found = getComponent(base, name, activeBinding);
                if (found) {
                    return found;
                }
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
    for (const Component* cur : globalScopeComponents) {
        if (cur->getComponentType()->getName() == toString(boundName)) {
            return cur;
        }
    }

    // no such component in scope
    return nullptr;
}

const Component* ComponentLookupAnalysis::getEnclosingComponent(const Component* comp) const {
    const auto found = enclosingComponent.find(comp);
    const Component* const enclosing = (found == enclosingComponent.end() ? nullptr : found->second);
    return enclosing;
}

}  // namespace souffle::ast::analysis
