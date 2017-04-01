/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentModel.cpp
 *
 ***********************************************************************/

#include "ComponentModel.h"
#include "AstComponent.h"
#include "AstProgram.h"
#include "AstVisitor.h"
#include "ErrorReport.h"

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
    AstTypeIdentifier boundName = activeBinding.find(name);
    if (boundName.empty()) {
        // compName is not bound to anything => just just compName
        boundName = name;
    }

    // search nested scopes bottom up
    const AstComponent* searchScope = scope;
    while (searchScope != nullptr) {
        for (const AstComponent* cur : searchScope->getComponents()) {
            if (cur->getComponentType().getName() == toString(boundName)) {
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
        if (cur->getComponentType().getName() == toString(boundName)) {
            return cur;
        }
    }

    // no such component in scope
    return nullptr;
}

namespace {

static const unsigned int MAX_INSTANTIATION_DEPTH = 1000;

/**
 * A container type for the (instantiated) content of a component.
 */
struct ComponentContent {
    std::vector<std::unique_ptr<AstType>> types;
    std::vector<std::unique_ptr<AstRelation>> relations;
    std::vector<std::unique_ptr<AstIODirective>> ioDirectives;

    void add(std::unique_ptr<AstType>& type, ErrorReport& report) {
        // add to result content (check existence first)
        auto foundItem =
                std::find_if(types.begin(), types.end(), [&](const std::unique_ptr<AstType>& element) {
                    return (element->getName() == type->getName());
                });
        if (foundItem != types.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage("Redefinition of type " + toString(type->getName()), type->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        types.push_back(std::move(type));
    }

    void add(std::unique_ptr<AstRelation>& rel, ErrorReport& report) {
        // add to result content (check existence first)
        auto foundItem = std::find_if(
                relations.begin(), relations.end(), [&](const std::unique_ptr<AstRelation>& element) {
                    return (element->getName() == rel->getName());
                });
        if (foundItem != relations.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage(
                                   "Redefinition of relation " + toString(rel->getName()), rel->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        relations.push_back(std::move(rel));
    }

    void add(std::unique_ptr<AstIODirective>& io, ErrorReport& report) {
        // add to result content (check existence of non-output directives first)
        auto foundItem = std::find_if(ioDirectives.begin(), ioDirectives.end(),
                [&](const std::unique_ptr<AstIODirective>& element) {
                    return (!element->isOutput() && element->getName() == io->getName() &&
                            element->isInput() == io->isInput() &&
                            element->isPrintSize() == io->isPrintSize());
                });
        if (foundItem != ioDirectives.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage("Redefinition of IO directive " + toString(io->getName()),
                                   io->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        ioDirectives.push_back(std::move(io));
    }
};

/**
 * Recursively computes the set of relations (and included clauses) introduced
 * by this init statement enclosed within the given scope.
 */
ComponentContent getInstantiatedContent(const AstComponentInit& componentInit,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup,
        std::vector<std::unique_ptr<AstClause>>& orphans, ErrorReport& report,
        const TypeBinding& binding = TypeBinding(), unsigned int maxDepth = MAX_INSTANTIATION_DEPTH);

/**
 * Collects clones of all the content in the given component and its base components.
 */
void collectContent(const AstComponent& component, const TypeBinding& binding,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup, ComponentContent& res,
        std::vector<std::unique_ptr<AstClause>>& orphans, std::set<std::string> overridden,
        ErrorReport& report, unsigned int maxInstantiationDepth) {
    // start with relations and clauses of the base components
    for (const auto& base : component.getBaseComponents()) {
        const AstComponent* comp = componentLookup.getComponent(enclosingComponent, base.getName(), binding);
        if (comp) {
            // link formal with actual type parameters
            const auto& formalParams = comp->getComponentType().getTypeParameters();
            const auto& actualParams = base.getTypeParameters();

            // update type binding
            TypeBinding activeBinding = binding.extend(formalParams, actualParams);

            for (const auto& cur : comp->getInstantiations()) {
                // instantiate sub-component
                ComponentContent content = getInstantiatedContent(*cur, enclosingComponent, componentLookup,
                        orphans, report, activeBinding, maxInstantiationDepth - 1);

                // process types
                for (auto& type : content.types) {
                    res.add(type, report);
                }

                // process relations
                for (auto& rel : content.relations) {
                    res.add(rel, report);
                }

                // process io directives
                for (auto& io : content.ioDirectives) {
                    res.add(io, report);
                }
            }

            // collect definitions from base type
            std::set<std::string> superOverridden;
            superOverridden.insert(overridden.begin(), overridden.end());
            superOverridden.insert(component.getOverridden().begin(), component.getOverridden().end());
            collectContent(*comp, activeBinding, comp, componentLookup, res, orphans, superOverridden, report,
                    maxInstantiationDepth);
        }
    }

    // and continue with the local types
    for (const auto& cur : component.getTypes()) {
        // create a clone
        std::unique_ptr<AstType> type(cur->clone());

        // instantiate elements of union types
        visitDepthFirst(*type, [&](const AstUnionType& type) {
            for (const auto& name : type.getTypes()) {
                AstTypeIdentifier newName = binding.find(name);
                if (!newName.empty()) {
                    const_cast<AstTypeIdentifier&>(name) = newName;
                }
            }
        });

        // instantiate elements of record types
        visitDepthFirst(*type, [&](const AstRecordType& type) {
            for (const auto& field : type.getFields()) {
                AstTypeIdentifier newName = binding.find(field.type);
                if (!newName.empty()) {
                    const_cast<AstTypeIdentifier&>(field.type) = newName;
                }
            }
        });

        // add to result list (check existence first)
        res.add(type, report);
    }

    // and the local relations
    for (const auto& cur : component.getRelations()) {
        // create a clone
        std::unique_ptr<AstRelation> rel(cur->clone());

        // update attribute types
        for (AstAttribute* attr : rel->getAttributes()) {
            AstTypeIdentifier forward = binding.find(attr->getTypeName());
            if (!forward.empty()) {
                attr->setTypeName(forward);
            }
        }

        // add to result list (check existence first)
        res.add(rel, report);
    }

    // and the local io directives
    for (const auto& cur : component.getIODirectives()) {
        // create a clone
        std::unique_ptr<AstIODirective> io(cur->clone());

        res.add(io, report);
    }

    // index the available relations
    std::map<AstRelationIdentifier, AstRelation*> index;
    for (const auto& cur : res.relations) {
        index[cur->getName()] = cur.get();
    }

    // add the local clauses
    for (const auto& cur : component.getClauses()) {
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
    for (auto iter = orphans.begin(); iter != orphans.end();) {
        auto& cur = *iter;
        AstRelation* rel = index[cur->getHead()->getName()];
        if (rel) {
            // add orphan to current instance and delete from orphan list
            rel->addClause(std::unique_ptr<AstClause>(cur->clone()));
            iter = orphans.erase(iter);
        } else {
            ++iter;
        }
    }
}

ComponentContent getInstantiatedContent(const AstComponentInit& componentInit,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup,
        std::vector<std::unique_ptr<AstClause>>& orphans, ErrorReport& report, const TypeBinding& binding,
        unsigned int maxDepth) {
    // start with an empty list
    ComponentContent res;

    if (maxDepth == 0) {
        report.addError("Component instantiation limit reached", componentInit.getSrcLoc());
        return res;
    }

    // get referenced component
    const AstComponent* component = componentLookup.getComponent(
            enclosingComponent, componentInit.getComponentType().getName(), binding);
    if (!component) {
        // this component is not defined => will trigger a semantic error
        return res;
    }

    // update type biding
    const auto& formalParams = component->getComponentType().getTypeParameters();
    const auto& actualParams = componentInit.getComponentType().getTypeParameters();
    TypeBinding activeBinding = binding.extend(formalParams, actualParams);

    // instantiated nested components
    for (const auto& cur : component->getInstantiations()) {
        // get nested content
        ComponentContent nestedContent = getInstantiatedContent(
                *cur, component, componentLookup, orphans, report, activeBinding, maxDepth - 1);

        // add types
        for (auto& type : nestedContent.types) {
            res.add(type, report);
        }

        // add relations
        for (auto& rel : nestedContent.relations) {
            res.add(rel, report);
        }

        // add IO directives
        for (auto& io : nestedContent.ioDirectives) {
            res.add(io, report);
        }
    }

    // collect all content in this component
    std::set<std::string> overridden;
    collectContent(*component, activeBinding, enclosingComponent, componentLookup, res, orphans, overridden,
            report, maxDepth);

    // update type names
    std::map<AstTypeIdentifier, AstTypeIdentifier> typeNameMapping;
    for (const auto& cur : res.types) {
        auto newName = componentInit.getInstanceName() + cur->getName();
        typeNameMapping[cur->getName()] = newName;
        cur->setName(newName);
    }

    // update relation names
    std::map<AstRelationIdentifier, AstRelationIdentifier> relationNameMapping;
    for (const auto& cur : res.relations) {
        auto newName = componentInit.getInstanceName() + cur->getName();
        relationNameMapping[cur->getName()] = newName;
        cur->setName(newName);
    }

    // create a helper function fixing type and relation references
    auto fixNames = [&](const AstNode& node) {

        // rename attribute types in headers
        visitDepthFirst(node, [&](const AstAttribute& attr) {
            auto pos = typeNameMapping.find(attr.getTypeName());
            if (pos != typeNameMapping.end()) {
                const_cast<AstAttribute&>(attr).setTypeName(pos->second);
            }
        });

        // rename atoms in clauses
        visitDepthFirst(node, [&](const AstAtom& atom) {
            auto pos = relationNameMapping.find(atom.getName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstAtom&>(atom).setName(pos->second);
            }
        });

        // rename IO directives
        visitDepthFirst(node, [&](const AstIODirective& ioDirective) {
            auto pos = relationNameMapping.find(ioDirective.getName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstIODirective&>(ioDirective).setName(pos->second);
            }
        });
    };

    // rename attribute type in headers and atoms in clauses of the relation
    for (const auto& cur : res.relations) {
        fixNames(*cur);
    }

    // rename orphans
    for (const auto& cur : orphans) {
        fixNames(*cur);
    }

    // rename orphans
    for (const auto& cur : res.ioDirectives) {
        fixNames(*cur);
    }

    // done
    return res;
}
}  // namespace

bool ComponentInstantiationTransformer::transform(AstTranslationUnit& translationUnit) {
    // TODO: Do this without being a friend class of AstProgram

    // unbound clauses with no relation defined
    std::vector<std::unique_ptr<AstClause>> unbound;

    AstProgram& program = *translationUnit.getProgram();

    ComponentLookup* componentLookup = translationUnit.getAnalysis<ComponentLookup>();

    for (const auto& cur : program.instantiations) {
        std::vector<std::unique_ptr<AstClause>> orphans;

        ComponentContent content = getInstantiatedContent(
                *cur, nullptr, *componentLookup, orphans, translationUnit.getErrorReport());
        for (auto& type : content.types) {
            program.types.insert(std::make_pair(type->getName(), std::move(type)));
        }
        for (auto& rel : content.relations) {
            program.relations.insert(std::make_pair(rel->getName(), std::move(rel)));
        }
        for (auto& io : content.ioDirectives) {
            program.ioDirectives.push_back(std::move(io));
        }
        for (auto& cur : orphans) {
            auto pos = program.relations.find(cur->getHead()->getName());
            if (pos != program.relations.end()) {
                pos->second->addClause(std::move(cur));
            } else {
                unbound.push_back(std::move(cur));
            }
        }
    }

    // add clauses
    for (auto& cur : program.clauses) {
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

    // unbound directives with no relation defined
    std::vector<std::unique_ptr<AstIODirective>> unboundDirectives;

    // add IO directives
    for (auto& cur : program.ioDirectives) {
        auto pos = program.relations.find(cur->getName());
        if (pos != program.relations.end()) {
            pos->second->addIODirectives(std::move(cur));
        } else {
            unboundDirectives.push_back(std::move(cur));
        }
    }
    // remember the remaining orphan directives
    program.ioDirectives.clear();
    program.ioDirectives.swap(unboundDirectives);

    return true;
}

}  // end namespace souffle
