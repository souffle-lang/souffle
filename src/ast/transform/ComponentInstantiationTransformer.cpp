/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentInstantiationTransformer.cpp
 *
 * Instantiate Components
 *
 ***********************************************************************/

#include "ast/transform/ComponentInstantiationTransformer.h"
#include "ErrorReport.h"
#include "ast/AstArgument.h"
#include "ast/AstAttribute.h"
#include "ast/AstClause.h"
#include "ast/AstComponent.h"
#include "ast/AstIO.h"
#include "ast/AstLiteral.h"
#include "ast/AstProgram.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstType.h"
#include "ast/AstVisitor.h"
#include "ast/analysis/ComponentLookupAnalysis.h"
#include "utility/StringUtil.h"
#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

class AstNode;

namespace {

static const unsigned int MAX_INSTANTIATION_DEPTH = 1000;

/**
 * A container type for the (instantiated) content of a component.
 */
struct ComponentContent {
    std::vector<std::unique_ptr<AstType>> types;
    std::vector<std::unique_ptr<AstRelation>> relations;
    std::vector<std::unique_ptr<AstIO>> ios;
    std::vector<std::unique_ptr<AstClause>> clauses;

    void add(std::unique_ptr<AstType>& type, ErrorReport& report) {
        // add to result content (check existence first)
        auto foundItem =
                std::find_if(types.begin(), types.end(), [&](const std::unique_ptr<AstType>& element) {
                    return (element->getQualifiedName() == type->getQualifiedName());
                });
        if (foundItem != types.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage(
                            "Redefinition of type " + toString(type->getQualifiedName()), type->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        types.push_back(std::move(type));
    }

    void add(std::unique_ptr<AstRelation>& rel, ErrorReport& report) {
        // add to result content (check existence first)
        auto foundItem = std::find_if(
                relations.begin(), relations.end(), [&](const std::unique_ptr<AstRelation>& element) {
                    return (element->getQualifiedName() == rel->getQualifiedName());
                });
        if (foundItem != relations.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage("Redefinition of relation " + toString(rel->getQualifiedName()),
                            rel->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        relations.push_back(std::move(rel));
    }

    void add(std::unique_ptr<AstClause>& clause, ErrorReport& /* report */) {
        clauses.push_back(std::move(clause));
    }

    void add(std::unique_ptr<AstIO>& newIO, ErrorReport& report) {
        // Check if i/o directive already exists
        auto foundItem = std::find_if(ios.begin(), ios.end(), [&](const std::unique_ptr<AstIO>& io) {
            return io->getQualifiedName() == newIO->getQualifiedName();
        });
        // if yes, add error
        if (foundItem != ios.end()) {
            auto type = (*foundItem)->getType();
            if (type == newIO->getType() && newIO->getType() != AstIoType::output) {
                Diagnostic err(Diagnostic::ERROR,
                        DiagnosticMessage("Redefinition I/O operation " + toString(newIO->getQualifiedName()),
                                newIO->getSrcLoc()),
                        {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
                report.addDiagnostic(err);
            }
        }
        // if not, add it
        ios.push_back(std::move(newIO));
    }
};

/**
 * Recursively computes the set of relations (and included clauses) introduced
 * by this init statement enclosed within the given scope.
 */
ComponentContent getInstantiatedContent(AstProgram& program, const AstComponentInit& componentInit,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup,
        std::vector<std::unique_ptr<AstClause>>& orphans, ErrorReport& report,
        const TypeBinding& binding = TypeBinding(), unsigned int maxDepth = MAX_INSTANTIATION_DEPTH);

/**
 * Collects clones of all the content in the given component and its base components.
 */
void collectContent(AstProgram& program, const AstComponent& component, const TypeBinding& binding,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup, ComponentContent& res,
        std::vector<std::unique_ptr<AstClause>>& orphans, const std::set<std::string>& overridden,
        ErrorReport& report, unsigned int maxInstantiationDepth) {
    // start with relations and clauses of the base components
    for (const auto& base : component.getBaseComponents()) {
        const AstComponent* comp = componentLookup.getComponent(enclosingComponent, base->getName(), binding);
        if (comp != nullptr) {
            // link formal with actual type parameters
            const auto& formalParams = comp->getComponentType()->getTypeParameters();
            const auto& actualParams = base->getTypeParameters();

            // update type binding
            TypeBinding activeBinding = binding.extend(formalParams, actualParams);

            for (const auto& cur : comp->getInstantiations()) {
                // instantiate sub-component
                ComponentContent content = getInstantiatedContent(program, *cur, enclosingComponent,
                        componentLookup, orphans, report, activeBinding, maxInstantiationDepth - 1);

                // process types
                for (auto& type : content.types) {
                    res.add(type, report);
                }

                // process relations
                for (auto& rel : content.relations) {
                    res.add(rel, report);
                }

                // process clauses
                for (auto& clause : content.clauses) {
                    res.add(clause, report);
                }

                // process io directives
                for (auto& io : content.ios) {
                    res.add(io, report);
                }
            }

            // collect definitions from base type
            std::set<std::string> superOverridden;
            superOverridden.insert(overridden.begin(), overridden.end());
            superOverridden.insert(component.getOverridden().begin(), component.getOverridden().end());
            collectContent(program, *comp, activeBinding, comp, componentLookup, res, orphans,
                    superOverridden, report, maxInstantiationDepth);
        }
    }

    // and continue with the local types
    for (const auto& cur : component.getTypes()) {
        // create a clone
        std::unique_ptr<AstType> type(cur->clone());

        // instantiate elements of union types
        visitDepthFirst(*type, [&](const AstUnionType& type) {
            for (const auto& name : type.getTypes()) {
                AstQualifiedName newName = binding.find(name);
                if (!newName.empty()) {
                    const_cast<AstQualifiedName&>(name) = newName;
                }
            }
        });

        // instantiate elements of record types
        visitDepthFirst(*type, [&](const AstRecordType& type) {
            for (const auto& field : type.getFields()) {
                auto&& newName = binding.find(field->getTypeName());
                if (!newName.empty()) {
                    const_cast<AstAttribute&>(*field).setTypeName(newName);
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
            AstQualifiedName forward = binding.find(attr->getTypeName());
            if (!forward.empty()) {
                attr->setTypeName(forward);
            }
        }

        // add to result list (check existence first)
        res.add(rel, report);
    }

    // and the local io directives
    for (const auto& io : component.getIOs()) {
        // create a clone
        std::unique_ptr<AstIO> instantiatedIO(io->clone());

        res.add(instantiatedIO, report);
    }

    // index the available relations
    std::map<AstQualifiedName, AstRelation*> index;
    for (const auto& cur : res.relations) {
        index[cur->getQualifiedName()] = cur.get();
    }

    // add the local clauses
    // TODO: check orphans
    for (const auto& cur : component.getClauses()) {
        if (overridden.count(cur->getHead()->getQualifiedName().getQualifiers()[0]) == 0) {
            AstRelation* rel = index[cur->getHead()->getQualifiedName()];
            if (rel != nullptr) {
                std::unique_ptr<AstClause> instantiatedClause(cur->clone());
                res.add(instantiatedClause, report);
            } else {
                orphans.emplace_back(cur->clone());
            }
        }
    }

    // add orphan clauses at the current level if they can be resolved
    for (auto iter = orphans.begin(); iter != orphans.end();) {
        auto& cur = *iter;
        AstRelation* rel = index[cur->getHead()->getQualifiedName()];
        if (rel != nullptr) {
            // add orphan to current instance and delete from orphan list
            std::unique_ptr<AstClause> instantiatedClause(cur->clone());
            res.add(instantiatedClause, report);
            iter = orphans.erase(iter);
        } else {
            ++iter;
        }
    }
}

ComponentContent getInstantiatedContent(AstProgram& program, const AstComponentInit& componentInit,
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
            enclosingComponent, componentInit.getComponentType()->getName(), binding);
    if (component == nullptr) {
        // this component is not defined => will trigger a semantic error
        return res;
    }

    // update type biding
    const auto& formalParams = component->getComponentType()->getTypeParameters();
    const auto& actualParams = componentInit.getComponentType()->getTypeParameters();
    TypeBinding activeBinding = binding.extend(formalParams, actualParams);

    // instantiated nested components
    for (const auto& cur : component->getInstantiations()) {
        // get nested content
        ComponentContent nestedContent = getInstantiatedContent(
                program, *cur, component, componentLookup, orphans, report, activeBinding, maxDepth - 1);

        // add types
        for (auto& type : nestedContent.types) {
            res.add(type, report);
        }

        // add relations
        for (auto& rel : nestedContent.relations) {
            res.add(rel, report);
        }

        // add clauses
        for (auto& clause : nestedContent.clauses) {
            res.add(clause, report);
        }

        // add IO directives
        for (auto& io : nestedContent.ios) {
            res.add(io, report);
        }
    }

    // collect all content in this component
    std::set<std::string> overridden;
    collectContent(program, *component, activeBinding, enclosingComponent, componentLookup, res, orphans,
            overridden, report, maxDepth);

    // update type names
    std::map<AstQualifiedName, AstQualifiedName> typeNameMapping;
    for (const auto& cur : res.types) {
        auto newName = componentInit.getInstanceName() + cur->getQualifiedName();
        typeNameMapping[cur->getQualifiedName()] = newName;
        cur->setQualifiedName(newName);
    }

    // update relation names
    std::map<AstQualifiedName, AstQualifiedName> relationNameMapping;
    for (const auto& cur : res.relations) {
        auto newName = componentInit.getInstanceName() + cur->getQualifiedName();
        relationNameMapping[cur->getQualifiedName()] = newName;
        cur->setQualifiedName(newName);
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
            auto pos = relationNameMapping.find(atom.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstAtom&>(atom).setQualifiedName(pos->second);
            }
        });

        // rename IO directives
        visitDepthFirst(node, [&](const AstIO& io) {
            auto pos = relationNameMapping.find(io.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstIO&>(io).setQualifiedName(pos->second);
            }
        });

        // rename field types in records
        visitDepthFirst(node, [&](const AstRecordType& recordType) {
            auto&& fields = recordType.getFields();
            for (size_t i = 0; i < fields.size(); i++) {
                auto& field = fields[i];
                auto pos = typeNameMapping.find(field->getTypeName());
                if (pos != typeNameMapping.end()) {
                    const_cast<AstRecordType&>(recordType).setFieldType(i, pos->second);
                }
            }
        });

        // rename variant types in unions
        visitDepthFirst(node, [&](const AstUnionType& unionType) {
            auto& variants = unionType.getTypes();
            for (size_t i = 0; i < variants.size(); i++) {
                auto pos = typeNameMapping.find(variants[i]);
                if (pos != typeNameMapping.end()) {
                    const_cast<AstUnionType&>(unionType).setVariantType(i, pos->second);
                }
            }
        });

        // rename type information in typecast
        visitDepthFirst(node, [&](const AstTypeCast& cast) {
            auto pos = typeNameMapping.find(cast.getType());
            if (pos != typeNameMapping.end()) {
                const_cast<AstTypeCast&>(cast).setType(pos->second);
            }
        });
    };

    // rename attributes in relation decls
    for (const auto& cur : res.relations) {
        fixNames(*cur);
    }

    // rename added clauses
    for (const auto& cur : res.clauses) {
        fixNames(*cur);
    }

    // rename orphans
    for (const auto& cur : orphans) {
        fixNames(*cur);
    }

    // rename io directives
    for (const auto& cur : res.ios) {
        fixNames(*cur);
    }

    // rename subtypes
    for (const auto& cur : res.types) {
        fixNames(*cur);
    }

    // done
    return res;
}
}  // namespace

bool ComponentInstantiationTransformer::transform(AstTranslationUnit& translationUnit) {
    // TODO: Do this without being a friend class of AstProgram

    AstProgram& program = *translationUnit.getProgram();

    auto* componentLookup = translationUnit.getAnalysis<ComponentLookup>();

    for (const auto& cur : program.instantiations) {
        std::vector<std::unique_ptr<AstClause>> orphans;

        ComponentContent content = getInstantiatedContent(
                program, *cur, nullptr, *componentLookup, orphans, translationUnit.getErrorReport());
        for (auto& type : content.types) {
            program.types.push_back(std::move(type));
        }
        for (auto& rel : content.relations) {
            program.relations.push_back(std::move(rel));
        }
        for (auto& clause : content.clauses) {
            program.clauses.push_back(std::move(clause));
        }
        for (auto& orphan : orphans) {
            program.clauses.push_back(std::move(orphan));
        }
        for (auto& io : content.ios) {
            program.ios.push_back(std::move(io));
        }
    }

    // delete components and instantiations
    program.instantiations.clear();
    program.components.clear();

    return true;
}

}  // end namespace souffle
