/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentLookupAnalysis.h
 *
 ***********************************************************************/

#pragma once

#include "ast/AstQualifiedName.h"
#include "ast/analysis/AstAnalysis.h"
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

class AstComponent;
class AstTranslationUnit;

/**
 * Class that encapsulates std::map of types binding that comes from .init c = Comp<MyType>
 * Type binding in this example would be T->MyType if the component code is .comp Comp<T> ...
 */
class TypeBinding {
public:
    /**
     * Returns binding for given name or empty string if such binding does not exist.
     */
    const AstQualifiedName& find(const AstQualifiedName& name) const {
        const static AstQualifiedName unknown;
        auto pos = binding.find(name);
        if (pos == binding.end()) {
            return unknown;
        }
        return pos->second;
    }

    TypeBinding extend(const std::vector<AstQualifiedName>& formalParams,
            const std::vector<AstQualifiedName>& actualParams) const {
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
    std::map<AstQualifiedName, AstQualifiedName> binding;
};

class ComponentLookup : public AstAnalysis {
public:
    static constexpr const char* name = "component-lookup";

    ComponentLookup() : AstAnalysis(name) {}

    void run(const AstTranslationUnit& translationUnit) override;

    /**
     * Performs a lookup operation for a component with the given name within the addressed scope.
     *
     * @param scope the component scope to lookup in (null for global scope)
     * @param name the name of the component to be looking for
     * @return a pointer to the obtained component or null if there is no such component.
     */
    const AstComponent* getComponent(
            const AstComponent* scope, const std::string& name, const TypeBinding& activeBinding) const;

private:
    // components defined outside of any components
    std::set<const AstComponent*> globalScopeComponents;
    // components defined inside a component
    std::map<const AstComponent*, std::set<const AstComponent*>> nestedComponents;
    // component definition enclosing a component definition
    std::map<const AstComponent*, const AstComponent*> enclosingComponent;
};

}  // end of namespace souffle
