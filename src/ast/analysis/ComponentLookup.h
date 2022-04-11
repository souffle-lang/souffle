/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentLookup.h
 *
 ***********************************************************************/

#pragma once

#include "ast/Component.h"
#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

/**
 * Class that encapsulates std::map of types binding that comes from .init c = Comp<MyType>
 * Type binding in this example would be T->MyType if the component code is .comp Comp<T> ...
 */
class TypeBinding {
public:
    /**
     * Returns binding for given name or empty string if such binding does not exist.
     */
    const QualifiedName& find(const QualifiedName& name) const {
        const static QualifiedName unknown;
        auto pos = binding.find(name);
        if (pos == binding.end()) {
            return unknown;
        }
        return pos->second;
    }

    TypeBinding extend(const std::vector<QualifiedName>& formalParams,
            const std::vector<QualifiedName>& actualParams) const {
        TypeBinding result;
        if (formalParams.size() != actualParams.size()) {
            return *this;  // invalid init => will trigger a semantic error
        }

        for (std::size_t i = 0; i < formalParams.size(); i++) {
            auto pos = binding.find(actualParams[i]);
            if (pos != binding.end()) {
                result.binding[formalParams[i]] = pos->second;
            } else {
                result.binding[formalParams[i]] = actualParams[i];
            }
        }

        return result;
    }

private:
    /**
     * Key value pair. Keys are names that should be forwarded to value,
     * which is the actual name. Example T->MyImplementation.
     */
    std::map<QualifiedName, QualifiedName> binding;
};

class ComponentLookupAnalysis : public Analysis {
public:
    static constexpr const char* name = "component-lookup";

    ComponentLookupAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    /**
     * Performs a lookup operation for a component with the given name within the addressed scope.
     *
     * @param scope the component scope to lookup in (null for global scope)
     * @param name the name of the component to be looking for
     * @return a pointer to the obtained component or null if there is no such component.
     */
    const Component* getComponent(
            const Component* scope, const std::string& name, const TypeBinding& activeBinding) const;

    /**
     * Return the component enclosing the given component.
     *
     * @param comp the enclosed component.
     * @return a pointer to the enclosing component, or null if the component is in the global scope.
     */
    const Component* getEnclosingComponent(const Component* comp) const;

private:
    // components defined outside of any components
    std::set<const Component*> globalScopeComponents;
    // components defined inside a component
    std::map<const Component*, std::set<const Component*>> nestedComponents;
    // component definition enclosing a component definition
    std::map<const Component*, const Component*> enclosingComponent;
};

}  // namespace souffle::ast::analysis
