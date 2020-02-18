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

#include "ComponentInstantiationTransformer.h"
#include "AstAttribute.h"
#include "AstClause.h"
#include "AstComponent.h"
#include "AstIO.h"
#include "AstLiteral.h"
#include "AstProgram.h"
#include "AstQualifiedName.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"
#include "AstVisitor.h"
#include "ComponentLookupAnalysis.h"
#include "ErrorReport.h"
#include "Util.h"
#include <algorithm>
#include <memory>

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
    std::vector<std::unique_ptr<AstLoad>> loads;
    std::vector<std::unique_ptr<AstPrintSize>> printSizes;
    std::vector<std::unique_ptr<AstStore>> stores;

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

    void add(std::unique_ptr<AstLoad>& directive, ErrorReport& report) {
        // Check if load directive already exists
        auto foundItem = std::find_if(loads.begin(), loads.end(), [&](const std::unique_ptr<AstLoad>& load) {
            return load->getQualifiedName() == directive->getQualifiedName();
        });
        // if yes, add error
        if (foundItem != loads.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage(
                            "Redefinition of IO directive " + toString(directive->getQualifiedName()),
                            directive->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        // if not, add it
        loads.push_back(std::move(directive));
    }

    void add(std::unique_ptr<AstPrintSize>& directive, ErrorReport& report) {
        // Check if load directive already exists
        auto foundItem = std::find_if(
                printSizes.begin(), printSizes.end(), [&](const std::unique_ptr<AstPrintSize>& printSize) {
                    return printSize->getQualifiedName() == directive->getQualifiedName();
                });
        // if yes, add error
        if (foundItem != printSizes.end()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage(
                            "Redefinition of IO directive " + toString(directive->getQualifiedName()),
                            directive->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        // if not, add it
        printSizes.push_back(std::move(directive));
    }

    void add(std::unique_ptr<AstStore>& directive, ErrorReport& report) {
        stores.push_back(std::move(directive));
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
                for (auto& io : content.loads) {
                    res.add(io, report);
                }
                for (auto& io : content.printSizes) {
                    res.add(io, report);
                }
                for (auto& io : content.stores) {
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
                AstQualifiedName newName = binding.find(name);
                if (!newName.empty()) {
                    const_cast<AstQualifiedName&>(name) = newName;
                }
            }
        });

        // instantiate elements of record types
        visitDepthFirst(*type, [&](const AstRecordType& type) {
            for (const auto& field : type.getFields()) {
                AstQualifiedName newName = binding.find(field.type);
                if (!newName.empty()) {
                    const_cast<AstQualifiedName&>(field.type) = newName;
                }
            }
        });

        // instantiate elements of sum types
        visitDepthFirst(*type, [&](const AstSumType& type) {
            for (const auto& branch : type.getBranches()) {
                AstQualifiedName newName = binding.find(branch.type);
                if (!newName.empty()) {
                    const_cast<AstQualifiedName&>(branch.type) = std::move(newName);
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
    for (const auto& cur : component.getLoads()) {
        // create a clone
        std::unique_ptr<AstLoad> io(cur->clone());

        res.add(io, report);
    }
    for (const auto& cur : component.getPrintSizes()) {
        // create a clone
        std::unique_ptr<AstPrintSize> io(cur->clone());

        res.add(io, report);
    }
    for (const auto& cur : component.getStores()) {
        // create a clone
        std::unique_ptr<AstStore> io(cur->clone());

        res.add(io, report);
    }

    // index the available relations
    std::map<AstQualifiedName, AstRelation*> index;
    for (const auto& cur : res.relations) {
        index[cur->getQualifiedName()] = cur.get();
    }

    // add the local clauses
    for (const auto& cur : component.getClauses()) {
        if (overridden.count(cur->getHead()->getQualifiedName().getQualifiers()[0]) == 0) {
            AstRelation* rel = index[cur->getHead()->getQualifiedName()];
            if (rel != nullptr) {
                rel->addClause(std::unique_ptr<AstClause>(cur->clone()));
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
        for (auto& io : nestedContent.loads) {
            res.add(io, report);
        }
        for (auto& io : nestedContent.printSizes) {
            res.add(io, report);
        }
        for (auto& io : nestedContent.stores) {
            res.add(io, report);
        }
    }

    // collect all content in this component
    std::set<std::string> overridden;
    collectContent(*component, activeBinding, enclosingComponent, componentLookup, res, orphans, overridden,
            report, maxDepth);

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
        visitDepthFirst(node, [&](const AstLoad& load) {
            auto pos = relationNameMapping.find(load.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstLoad&>(load).setQualifiedName(pos->second);
            }
        });
        visitDepthFirst(node, [&](const AstPrintSize& printSize) {
            auto pos = relationNameMapping.find(printSize.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstPrintSize&>(printSize).setQualifiedName(pos->second);
            }
        });
        visitDepthFirst(node, [&](const AstStore& store) {
            auto pos = relationNameMapping.find(store.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                const_cast<AstStore&>(store).setQualifiedName(pos->second);
            }
        });

        // rename field types in records
        visitDepthFirst(node, [&](const AstRecordType& recordType) {
            auto& fields = recordType.getFields();
            for (size_t i = 0; i < fields.size(); i++) {
                auto& field = fields[i];
                auto pos = typeNameMapping.find(field.type);
                if (pos != typeNameMapping.end()) {
                    const_cast<AstRecordType&>(recordType).setFieldType(i, pos->second);
                }
            }
        });

        // rename branches in sum type
        // TODO: CODE-DUP: this seems identical to the work done in `collectContent`
        visitDepthFirst(node, [&](const AstSumType& sumType) {
            auto& branches = sumType.getBranches();
            for (size_t i = 0; i < branches.size(); i++) {
                auto& branch = branches[i];
                auto pos = typeNameMapping.find(branch.type);
                if (pos != typeNameMapping.end()) {
                    const_cast<AstSumType&>(sumType).setVariantType(i, pos->second);
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

        // rename type information in record-init
        visitDepthFirst(node, [&](const AstRecordInit& recordInit) {
            if (recordInit.type) {
                auto pos = typeNameMapping.find(*recordInit.type);
                if (pos != typeNameMapping.end()) {
                    const_cast<AstRecordInit&>(recordInit).type = pos->second;
                }
            }
        });

        // rename type information in record-init
        visitDepthFirst(node, [&](const AstSumInit& sumInit) {
            auto pos = typeNameMapping.find(sumInit.type);
            if (pos != typeNameMapping.end()) {
                const_cast<AstSumInit&>(sumInit).type = pos->second;
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

    // rename attribute type in headers and atoms in clauses of the relation
    for (const auto& cur : res.relations) {
        fixNames(*cur);
    }

    // rename orphans
    for (const auto& cur : orphans) {
        fixNames(*cur);
    }

    // rename io directives
    for (const auto& cur : res.loads) {
        fixNames(*cur);
    }
    for (const auto& cur : res.printSizes) {
        fixNames(*cur);
    }
    for (const auto& cur : res.stores) {
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

    // unbound clauses with no relation defined
    std::vector<std::unique_ptr<AstClause>> unbound;

    AstProgram& program = *translationUnit.getProgram();

    auto* componentLookup = translationUnit.getAnalysis<ComponentLookup>();

    for (const auto& cur : program.instantiations) {
        std::vector<std::unique_ptr<AstClause>> orphans;

        ComponentContent content = getInstantiatedContent(
                *cur, nullptr, *componentLookup, orphans, translationUnit.getErrorReport());
        for (auto& type : content.types) {
            program.types.insert(std::make_pair(type->getQualifiedName(), std::move(type)));
        }
        for (auto& rel : content.relations) {
            program.relations.insert(std::make_pair(rel->getQualifiedName(), std::move(rel)));
        }
        for (auto& io : content.loads) {
            program.loads.push_back(std::move(io));
        }
        for (auto& io : content.printSizes) {
            program.printSizes.push_back(std::move(io));
        }
        for (auto& io : content.stores) {
            program.stores.push_back(std::move(io));
        }
        for (auto& cur : orphans) {
            auto pos = program.relations.find(cur->getHead()->getQualifiedName());
            if (pos != program.relations.end()) {
                pos->second->addClause(std::move(cur));
            } else {
                unbound.push_back(std::move(cur));
            }
        }
    }

    // add clauses
    for (auto& cur : program.clauses) {
        auto pos = program.relations.find(cur->getHead()->getQualifiedName());
        if (pos != program.relations.end()) {
            pos->second->addClause(std::move(cur));
        } else {
            unbound.push_back(std::move(cur));
        }
    }
    // remember the remaining orphan clauses
    program.clauses.clear();
    program.clauses.swap(unbound);
    /*
        // unbound directives with no relation defined
        std::vector<std::unique_ptr<AstLoad>> unboundLoads;
        std::vector<std::unique_ptr<AstPrintSize>> unboundPrintSizes;
        std::vector<std::unique_ptr<AstStore>> unboundStores;

        // add IO directives
        for (auto& cur : program.loads) {
            auto pos = program.relations.find(cur->getName());
            if (pos != program.relations.end()) {
                pos->second->addLoad(std::move(cur));
            } else {
                unboundLoads.push_back(std::move(cur));
            }
        }
        // remember the remaining orphan directives
        program.loads.clear();
        program.loads.swap(unboundLoads);

        for (auto& cur : program.stores) {
            auto pos = program.relations.find(cur->getName());
            if (pos != program.relations.end()) {
                pos->second->addStore(std::move(cur));
            } else {
                unboundStores.push_back(std::move(cur));
            }
        }
        // remember the remaining orphan directives
        program.stores.clear();
        program.stores.swap(unboundStores);
    */
    // delete components and instantiations
    program.instantiations.clear();
    program.components.clear();

    return true;
}

}  // end namespace souffle
