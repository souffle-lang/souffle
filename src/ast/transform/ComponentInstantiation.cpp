/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ComponentInstantiation.cpp
 *
 * Instantiate Components
 *
 ***********************************************************************/

#include "ast/transform/ComponentInstantiation.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/ComponentType.h"
#include "ast/Directive.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordType.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "ast/TypeCast.h"
#include "ast/UnionType.h"
#include "ast/analysis/ComponentLookup.h"
#include "ast/utility/Visitor.h"
#include "reports/ErrorReport.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

using namespace analysis;

namespace {

static const unsigned int MAX_INSTANTIATION_DEPTH = 1000;

/**
 * A container type for the (instantiated) content of a component.
 */
struct ComponentContent {
    VecOwn<ast::Type> types;
    VecOwn<Relation> relations;
    VecOwn<Directive> directives;
    VecOwn<Clause> clauses;

    void add(Own<ast::Type>& type, ErrorReport& report) {
        // add to result content (check existence first)
        auto foundItem = std::find_if(types.begin(), types.end(), [&](const Own<ast::Type>& element) {
            return (element->getQualifiedName() == type->getQualifiedName());
        });
        if (foundItem != types.end()) {
            Diagnostic err(Diagnostic::Type::ERROR,
                    DiagnosticMessage(
                            "Redefinition of type " + toString(type->getQualifiedName()), type->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        types.push_back(std::move(type));
    }

    void add(Own<Relation>& rel, ErrorReport& report) {
        // add to result content (check existence first)
        auto foundItem = std::find_if(relations.begin(), relations.end(), [&](const Own<Relation>& element) {
            return (element->getQualifiedName() == rel->getQualifiedName());
        });
        if (foundItem != relations.end()) {
            Diagnostic err(Diagnostic::Type::ERROR,
                    DiagnosticMessage("Redefinition of relation " + toString(rel->getQualifiedName()),
                            rel->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
            report.addDiagnostic(err);
        }
        relations.push_back(std::move(rel));
    }

    void add(Own<Clause>& clause, ErrorReport& /* report */) {
        clauses.push_back(std::move(clause));
    }

    void add(Own<Directive>& newDirective, ErrorReport& report) {
        // Check if directive already exists
        auto foundItem =
                std::find_if(directives.begin(), directives.end(), [&](const Own<Directive>& directive) {
                    return directive->getQualifiedName() == newDirective->getQualifiedName();
                });
        // if yes, add error
        if (foundItem != directives.end()) {
            auto type = (*foundItem)->getType();
            if (type == newDirective->getType() && newDirective->getType() != ast::DirectiveType::output) {
                Diagnostic err(Diagnostic::Type::ERROR,
                        DiagnosticMessage(
                                "Redefinition I/O operation " + toString(newDirective->getQualifiedName()),
                                newDirective->getSrcLoc()),
                        {DiagnosticMessage("Previous definition", (*foundItem)->getSrcLoc())});
                report.addDiagnostic(err);
            }
        }
        // if not, add it
        directives.push_back(std::move(newDirective));
    }
};

/**
 * Recursively computes the set of relations (and included clauses) introduced
 * by this init statement enclosed within the given scope.
 */
ComponentContent getInstantiatedContent(Program& program, const ComponentInit& componentInit,
        const Component* enclosingComponent, const ComponentLookupAnalysis& componentLookup,
        VecOwn<Clause>& orphans, ErrorReport& report, const TypeBinding& binding = analysis::TypeBinding(),
        unsigned int maxDepth = MAX_INSTANTIATION_DEPTH);

/**
 * Collects clones of all the content in the given component and its base components.
 */
void collectContent(Program& program, const Component& component, const TypeBinding& binding,
        const Component* enclosingComponent, const ComponentLookupAnalysis& componentLookup,
        ComponentContent& res, VecOwn<Clause>& orphans, const std::set<std::string>& overridden,
        ErrorReport& report, unsigned int maxInstantiationDepth) {
    // start with relations and clauses of the base components
    for (const auto& base : component.getBaseComponents()) {
        const Component* comp = componentLookup.getComponent(enclosingComponent, base->getName(), binding);
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

                // process directive directives
                for (auto& directive : content.directives) {
                    res.add(directive, report);
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
        Own<ast::Type> type(clone(cur));

        // instantiate user-defined type names
        visit(*type, [&](ast::Type& type) {
            auto&& newName = binding.find(type.getQualifiedName());
            if (!newName.empty()) {
                type.setQualifiedName(newName);
            }
        });

        // instantiate sub-type declarations
        visit(*type, [&](ast::SubsetType& type) {
            auto&& newName = binding.find(type.getBaseType());
            if (!newName.empty()) {
                type.setBaseType(newName);
            }
        });

        // instantiate elements of union types
        visit(*type, [&](ast::UnionType& type) {
            for (auto& name : type.getTypes()) {
                QualifiedName newName = binding.find(name);
                if (!newName.empty()) {
                    name = std::move(newName);
                }
            }
        });

        // instantiate elements of record types
        visit(*type, [&](ast::RecordType& type) {
            for (auto& field : type.getFields()) {
                auto&& newName = binding.find(field->getTypeName());
                if (!newName.empty()) {
                    field->setTypeName(newName);
                }
            }
        });

        // instantiate elements of ADT branch types
        visit(*type, [&](ast::BranchType& type) {
            // TODO(b-scholz): instiantiate branch identifier as well
            //                 (needs to be re-defined as QualifiedName)
            for (auto& field : type.getFields()) {
                auto&& newName = binding.find(field->getTypeName());
                if (!newName.empty()) {
                    field->setTypeName(newName);
                }
            }
        });

        // add to result list (check existence first)
        res.add(type, report);
    }

    // and the local relations
    for (const auto& cur : component.getRelations()) {
        // create a clone
        Own<Relation> rel(clone(cur));

        // update attribute types
        for (Attribute* attr : rel->getAttributes()) {
            QualifiedName forward = binding.find(attr->getTypeName());
            if (!forward.empty()) {
                attr->setTypeName(forward);
            }
        }

        // add to result list (check existence first)
        res.add(rel, report);
    }

    // and the local directive directives
    for (const auto& directive : component.getDirectives()) {
        // create a clone
        Own<Directive> instantiatedIO(clone(directive));

        res.add(instantiatedIO, report);
    }

    // index the available relations
    std::map<QualifiedName, Relation*> index;
    for (const auto& cur : res.relations) {
        index[cur->getQualifiedName()] = cur.get();
    }

    // add the local clauses
    // TODO: check orphans
    for (const auto& cur : component.getClauses()) {
        if (overridden.count(cur->getHead()->getQualifiedName().getQualifiers()[0]) == 0) {
            Relation* rel = index[cur->getHead()->getQualifiedName()];
            if (rel != nullptr) {
                Own<Clause> instantiatedClause(clone(cur));
                res.add(instantiatedClause, report);
            } else {
                orphans.emplace_back(clone(cur));
            }
        }
    }

    // add orphan clauses at the current level if they can be resolved
    for (auto iter = orphans.begin(); iter != orphans.end();) {
        auto& cur = *iter;
        Relation* rel = index[cur->getHead()->getQualifiedName()];
        if (rel != nullptr) {
            // add orphan to current instance and delete from orphan list
            Own<Clause> instantiatedClause(clone(cur));
            res.add(instantiatedClause, report);
            iter = orphans.erase(iter);
        } else {
            ++iter;
        }
    }
}

ComponentContent getInstantiatedContent(Program& program, const ComponentInit& componentInit,
        const Component* enclosingComponent, const ComponentLookupAnalysis& componentLookup,
        VecOwn<Clause>& orphans, ErrorReport& report, const TypeBinding& binding, unsigned int maxDepth) {
    // start with an empty list
    ComponentContent res;

    if (maxDepth == 0) {
        report.addError("Component instantiation limit reached", componentInit.getSrcLoc());
        return res;
    }

    // get referenced component
    const Component* component = componentLookup.getComponent(
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

        // add directives
        for (auto& directive : nestedContent.directives) {
            res.add(directive, report);
        }
    }

    // collect all content in this component
    std::set<std::string> overridden;
    collectContent(program, *component, activeBinding, enclosingComponent, componentLookup, res, orphans,
            overridden, report, maxDepth);

    // update type names
    std::map<QualifiedName, QualifiedName> typeNameMapping;
    for (const auto& cur : res.types) {
        auto newName = componentInit.getInstanceName() + cur->getQualifiedName();
        typeNameMapping[cur->getQualifiedName()] = newName;
        cur->setQualifiedName(newName);
    }

    // update relation names
    std::map<QualifiedName, QualifiedName> relationNameMapping;
    for (const auto& cur : res.relations) {
        auto newName = componentInit.getInstanceName() + cur->getQualifiedName();
        relationNameMapping[cur->getQualifiedName()] = newName;
        cur->setQualifiedName(newName);
    }

    // create a helper function fixing type and relation references
    auto fixNames = [&](Node& node) {
        // rename attribute types in headers
        visit(node, [&](Attribute& attr) {
            auto pos = typeNameMapping.find(attr.getTypeName());
            if (pos != typeNameMapping.end()) {
                attr.setTypeName(pos->second);
            }
        });

        // rename atoms in clauses
        visit(node, [&](Atom& atom) {
            auto pos = relationNameMapping.find(atom.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                atom.setQualifiedName(pos->second);
            }
        });

        // rename directives
        visit(node, [&](Directive& directive) {
            auto pos = relationNameMapping.find(directive.getQualifiedName());
            if (pos != relationNameMapping.end()) {
                directive.setQualifiedName(pos->second);
            }
        });

        // rename field types in records
        visit(node, [&](ast::RecordType& recordType) {
            auto&& fields = recordType.getFields();
            for (std::size_t i = 0; i < fields.size(); i++) {
                auto& field = fields[i];
                auto pos = typeNameMapping.find(field->getTypeName());
                if (pos != typeNameMapping.end()) {
                    recordType.setFieldType(i, pos->second);
                }
            }
        });

        // rename variant types in unions
        visit(node, [&](ast::UnionType& unionType) {
            auto& variants = unionType.getTypes();
            for (std::size_t i = 0; i < variants.size(); i++) {
                auto pos = typeNameMapping.find(variants[i]);
                if (pos != typeNameMapping.end()) {
                    unionType.setType(i, pos->second);
                }
            }
        });

        // rename type information in typecast
        visit(node, [&](ast::TypeCast& cast) {
            auto pos = typeNameMapping.find(cast.getType());
            if (pos != typeNameMapping.end()) {
                cast.setType(pos->second);
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

    // rename directive directives
    for (const auto& cur : res.directives) {
        fixNames(*cur);
    }

    // rename subtypes
    for (const auto& cur : res.types) {
        fixNames(*cur);
    }

    return res;
}
}  // namespace

bool ComponentInstantiationTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    auto& report = translationUnit.getErrorReport();

    auto* componentLookup = translationUnit.getAnalysis<ComponentLookupAnalysis>();

    for (const auto* cur : program.getComponentInstantiations()) {
        VecOwn<Clause> orphans;

        auto content = getInstantiatedContent(program, *cur, nullptr, *componentLookup, orphans, report);
        if (report.getNumErrors() != 0) continue;

        for (auto& type : content.types) {
            program.addType(std::move(type));
        }
        for (auto& rel : content.relations) {
            program.addRelation(std::move(rel));
        }
        for (auto& clause : content.clauses) {
            program.addClause(std::move(clause));
        }
        for (auto& orphan : orphans) {
            program.addClause(std::move(orphan));
        }
        for (auto& directive : content.directives) {
            program.addDirective(std::move(directive));
        }
    }

    // delete components and instantiations
    program.clearComponents();

    return true;
}

}  // namespace souffle::ast::transform
