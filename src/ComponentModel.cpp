/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All Rights reserved
 * 
 * The Universal Permissive License (UPL), Version 1.0
 * 
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the 
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified 
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 * 
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 * 
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and 
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the 
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 * 
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/************************************************************************
 *
 * @file ComponentInstantiation.cpp
 *
 ***********************************************************************/
#include "AstComponent.h"
#include "AstProgram.h"
#include "AstVisitor.h"
#include "ComponentModel.h"
#include "ErrorReport.h"

void ComponentLookup::run(const AstTranslationUnit& translationUnit) {
    const AstProgram *program = translationUnit.getProgram();
    for (AstComponent *component : program->getComponents()) {
        globalScopeComponents.insert(component);
        enclosingComponent[component] = nullptr;
    }
    visitDepthFirst(*program, [&](const AstComponent& cur) {
        nestedComponents[&cur];
        for (AstComponent *nestedComponent : cur.getComponents()) {
            nestedComponents[&cur].insert(nestedComponent);
            enclosingComponent[nestedComponent] = &cur;
        }
    });
}

const AstComponent* ComponentLookup::getComponent(const AstComponent* scope, const std::string& name, const TypeBinding& activeBinding) const {
    // forward according to binding (we do not do this recursively on purpose)
    std::string boundName = activeBinding.find(name);
    if (boundName.empty()) {
        // compName is not bound to anything => just just compName
        boundName = name;
    }

    // search nested scopes bottom up
    const AstComponent *searchScope = scope;
    while (searchScope != nullptr) {
        for(const AstComponent* cur : searchScope->getComponents()) {
            if (cur->getComponentType().getName() == boundName) return cur;
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
    for (const AstComponent *cur : globalScopeComponents) {
        if (cur->getComponentType().getName() == boundName) return cur;
    }

    // no such component in scope
    return nullptr;
}

bool ComponentInstantiationTransformer::transform(AstTranslationUnit& translationUnit) {
    // TODO: Do this without being a friend class of AstProgram

    // unbound clauses with no relation defined
    std::vector<std::unique_ptr<AstClause>> unbound;

    // type definitions in components 
    std::vector<std::unique_ptr<AstType>> types;

    AstProgram &program = *translationUnit.getProgram();

    ComponentLookup *componentLookup = translationUnit.getAnalysis<ComponentLookup>();

    for(const auto& cur : program.instantiations) {
        std::vector<std::unique_ptr<AstClause>> orphans;
        for(auto& rel : getInstantiatedRelations(*cur, nullptr, *componentLookup, orphans, types, translationUnit.getErrorReport())) {
            program.relations.insert(std::make_pair(rel->getName(), std::move(rel)));
        }
        for(auto& cur : orphans) {
            auto pos = program.relations.find(cur->getHead()->getName());
            if (pos != program.relations.end()) {
                pos->second->addClause(std::move(cur));
            } else {
                unbound.push_back(std::move(cur));
            }
        }
    }

    // add types 
    for(auto& type : types) {
        program.addType(std::move(type)); 
    } 

    // add clauses
    for(auto& cur : program.clauses) {
        auto pos = program.relations.find(cur->getHead()->getName());
        if (pos != program.relations.end()) {
            pos->second->addClause(std::move(cur));
        } else {
            unbound.push_back(std::move(cur));
        }
    }
    // remember the remaining orphan clauses
    program.clauses.clear();
    program.clauses.swap(unbound);

    return true;
}

std::vector<std::unique_ptr<AstRelation>> ComponentInstantiationTransformer::getInstantiatedRelations(const AstComponentInit &componentInit, const AstComponent *enclosingComponent, const ComponentLookup &componentLookup, std::vector<std::unique_ptr<AstClause>> &orphans, std::vector<std::unique_ptr<AstType>> &types, ErrorReport &report, const TypeBinding& binding, unsigned int maxDepth) {


    // start with an empty list
    std::vector<std::unique_ptr<AstRelation>> res;

    if (maxDepth == 0) {
        report.addError("Component instantiation limit reached", componentInit.getSrcLoc());
        return res;
    }

    // get referenced component
    const AstComponent* component = componentLookup.getComponent(enclosingComponent, componentInit.getComponentType().getName(), binding);
    if (!component) return res;         // this component is not defined => will trigger a semantic error

    // update type biding
    const auto& formalParams = component->getComponentType().getTypeParameters();
    const auto& actualParams = componentInit.getComponentType().getTypeParameters();
    TypeBinding activeBinding = binding.extend(formalParams, actualParams);

    // instantiated nested components
    for(const auto& cur : component->getInstantiations()) {
        for(auto& rel : getInstantiatedRelations(*cur, component, componentLookup, orphans, types, report, activeBinding, maxDepth - 1)) {
            // add to result list (check existence first)
            auto foundItem = std::find_if(res.begin(), res.end(),
                     [&](const std::unique_ptr<AstRelation> &element) { return (element->getName() == rel->getName()); });
            if(foundItem != res.end()) {
                Diagnostic err(Diagnostic::ERROR, DiagnosticMessage("Redefinition of relation " + toString(rel->getName()), rel->getSrcLoc()),
                        {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
                report.addDiagnostic(err);
            }
            res.push_back(std::move(rel));
        }
    }

    // collect all relations and clauses in this component
    std::set<std::string> overridden;
    collectAllRelations(*component, activeBinding, enclosingComponent, componentLookup, res, orphans, types, overridden, report, maxDepth);

    // update relation names
    std::map<AstRelationIdentifier,AstRelationIdentifier> mapping;
    for(const auto& cur : res) {
        auto newName = componentInit.getInstanceName() + cur->getName();
        mapping[cur->getName()] = newName;
        cur->setName(newName);
    }

    // rename atoms in clauses of the relation
    for(const auto& cur : res) {
        visitDepthFirst(*cur, [&](const AstAtom& atom) {
            auto pos = mapping.find(atom.getName());
            if (pos != mapping.end()) {
                const_cast<AstAtom&>(atom).setName(pos->second);
            }
        });
    }

    // rename orphans
    for(const auto& cur : orphans) {
        visitDepthFirst(*cur, [&](const AstAtom& atom) {
            auto pos = mapping.find(atom.getName());
            if (pos != mapping.end()) {
                const_cast<AstAtom&>(atom).setName(pos->second);
            }
        });
    }

    // add types from the component 
    for(const auto& cur : component->getTypes()) {
        // check for type equivalence 
        // add new type if not found
        auto pos = find_if(types.begin(), 
                           types.end(), 
                           [&] (const std::unique_ptr<AstType> &elem){return elem->getName() == cur->getName();}); 
        if(pos == types.end()) { 
            types.push_back(std::unique_ptr<AstType>(cur->clone())); 
        } else { 
           if(!(**pos == *cur)) {
               report.addError("Type " + cur->getName() + " redefined", component->getSrcLoc());
           } 
        } 
    } 

    // done
    return res;
}

void ComponentInstantiationTransformer::collectAllRelations(const AstComponent& component, const TypeBinding& binding, const AstComponent *enclosingComponent, const ComponentLookup &componentLookup, std::vector<std::unique_ptr<AstRelation>>& res, std::vector<std::unique_ptr<AstClause>> &orphans, std::vector<std::unique_ptr<AstType>> &types, std::set<std::string> overridden, ErrorReport &report, unsigned int maxInstantiationDepth) {

    // start with relations and clauses of the base components
    for(const auto& base : component.getBaseComponents()) {
        const AstComponent* comp = componentLookup.getComponent(enclosingComponent, base.getName(), binding);

        if (comp) {

            // link formal with actual type parameters
            const auto& formalParams = comp->getComponentType().getTypeParameters();
            const auto& actualParams = base.getTypeParameters();

            // update type binding
            TypeBinding activeBinding = binding.extend(formalParams, actualParams);

            for(const auto& cur : comp->getInstantiations()) {
                for(auto& rel : getInstantiatedRelations(*cur, enclosingComponent, componentLookup, orphans, types, report, activeBinding, maxInstantiationDepth - 1)) {
                    // add to result list (check existence first)
                    auto foundItem = std::find_if(res.begin(), res.end(),
                            [&](const std::unique_ptr<AstRelation> &element) { return (element->getName() == rel->getName()); });
                    if(foundItem != res.end()) {
                        Diagnostic err(Diagnostic::ERROR, DiagnosticMessage("Redefinition of relation " + toString(rel->getName()), rel->getSrcLoc()),
                                {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
                        report.addDiagnostic(err);
                    }
                    res.push_back(std::move(rel));
                }
            }

            // collect definitions from base type
            std::set<std::string> superOverridden;
            superOverridden.insert(overridden.begin(), overridden.end());
            superOverridden.insert(component.getOverridden().begin(), component.getOverridden().end());
            collectAllRelations(*comp, activeBinding, comp, componentLookup, res, orphans, types, superOverridden, report, maxInstantiationDepth);
        }
    }

    // and continue with the local once
    for(const auto& cur : component.getRelations()) {

        // create a clone
        std::unique_ptr<AstRelation> rel(cur->clone());

        // update attribute types
        for(AstAttribute *attr : rel->getAttributes()) {
            std::string forward = binding.find(attr->getTypeName());
            if (!forward.empty()) attr->setTypeName(forward);
        }

        // add to result list (check existence first)
        auto foundItem = std::find_if(res.begin(), res.end(),
                [&](const std::unique_ptr<AstRelation> &element) { return (element->getName() == rel->getName()); });
        if(foundItem != res.end()) {
            Diagnostic err(Diagnostic::ERROR, DiagnosticMessage("Redefinition of relation " + toString(rel->getName()), rel->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        res.push_back(std::move(rel));
    }

    // index the available relations
    std::map<AstRelationIdentifier,AstRelation*> index;
    for(const auto& cur : res) {
        index[cur->getName()] = cur.get();
    }

    // and finally add the local clauses
    for(const auto& cur : component.getClauses()) {
        if (overridden.count(cur->getHead()->getName().getNames()[0]) == 0) {
            AstRelation* rel = index[cur->getHead()->getName()];
            if (rel) {
                rel->addClause(std::unique_ptr<AstClause>(cur->clone()));
            } else {
                orphans.push_back(std::unique_ptr<AstClause>(cur->clone()));
            }
        }
    }

    // add orphan clauses at the current level if they can be resolved
    for(auto iter=orphans.begin(); iter != orphans.end(); ) {
        auto &cur = *iter;
        AstRelation* rel = index[cur->getHead()->getName()];
        if (rel) {
            // add orphan to current instance and delete from orphan list
            rel->addClause(std::unique_ptr<AstClause>(cur->clone()));
            iter = orphans.erase(iter);
        } else {
            ++iter;
        }
    }

    // add types from the component 
    for(const auto& cur : component.getTypes()) {
        auto pos = find_if(types.begin(), 
                           types.end(), 
                           [&] (const std::unique_ptr<AstType> &elem){return elem->getName() == cur->getName();}); 
        // check whether type does not exist yet
        if(pos == types.end()) { 
            types.push_back(std::unique_ptr<AstType>(cur->clone())); 
        } else { 
            if(!(**pos == *cur)) {
                report.addError("Type " + cur->getName() + " redefined", component.getSrcLoc());
            } 
        } 
    } 
}
