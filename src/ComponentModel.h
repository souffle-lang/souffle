/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentModel.h
 *
 ***********************************************************************/

#pragma once

#include "AstAnalysis.h"
#include "AstComponent.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"

#include <memory>
#include <string>
#include <vector>

namespace souffle {

class ErrorReport;

/**
 * Class that encapsulates std::map of types binding that comes from .init c = Comp<MyType>
 * Type binding in this example would be T->MyType if the component code is .comp Comp<T> ...
 */
class TypeBinding {
    /**
     * Key value pair. Keys are names that should be forwarded to value,
     * which is the actual name. Example T->MyImplementation.
     */
    std::map<AstTypeIdentifier, AstTypeIdentifier> binding;

public:
    /**
     * Returns binding for given name or empty string if such binding does not exist.
     */
    const AstTypeIdentifier& find(const AstTypeIdentifier& name) const {
        const static AstTypeIdentifier unknown;
        auto pos = binding.find(name);
        if (pos == binding.end()) {
            return unknown;
        }
        return pos->second;
    }

    TypeBinding extend(const std::vector<AstTypeIdentifier>& formalParams,
            const std::vector<AstTypeIdentifier>& actualParams) const {
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
};

class ComponentLookup : public AstAnalysis {
private:
    std::set<const AstComponent*> globalScopeComponents;  // components defined outside of any components
    std::map<const AstComponent*, std::set<const AstComponent*>>
            nestedComponents;  // components defined inside a component
    std::map<const AstComponent*, const AstComponent*>
            enclosingComponent;  // component definition enclosing a component definition
public:
    static constexpr const char* name = "component-lookup";

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
};

class ComponentInstantiationTransformer : public AstTransformer {
private:
    bool transform(AstTranslationUnit& translationUnit) override;

public:
    std::string getName() const override {
        return "ComponentInstantiationTransformer";
    }
};

}  // end of namespace souffle
