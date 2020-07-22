/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReplaceSingletonVariablesTransformer.cpp
 *
 ***********************************************************************/

#include "ast/transform/ReplaceSingletonVariables.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstUtils.h"
#include "ast/AstVisitor.h"
#include <memory>
#include <set>
#include <vector>

namespace souffle {
class AstRelation;

bool ReplaceSingletonVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    AstProgram& program = *translationUnit.getProgram();

    // Node-mapper to replace a set of singletons with unnamed variables
    struct replaceSingletons : public AstNodeMapper {
        std::set<std::string>& singletons;

        replaceSingletons(std::set<std::string>& singletons) : singletons(singletons) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* var = dynamic_cast<AstVariable*>(node.get())) {
                if (singletons.find(var->getName()) != singletons.end()) {
                    return std::make_unique<AstUnnamedVariable>();
                }
            }
            node->apply(*this);
            return node;
        }
    };

    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            std::set<std::string> nonsingletons;
            std::set<std::string> vars;

            visitDepthFirst(*clause, [&](const AstVariable& var) {
                const std::string& name = var.getName();
                if (vars.find(name) != vars.end()) {
                    // Variable seen before, so not a singleton variable
                    nonsingletons.insert(name);
                } else {
                    vars.insert(name);
                }
            });

            std::set<std::string> ignoredVars;

            // Don't unname singleton variables occurring in records.
            // TODO (azreika): remove this check once issue #420 is fixed
            std::set<std::string> recordVars;
            visitDepthFirst(*clause, [&](const AstRecordInit& rec) {
                visitDepthFirst(rec, [&](const AstVariable& var) { ignoredVars.insert(var.getName()); });
            });

            // Don't unname singleton variables occuring in constraints.
            std::set<std::string> constraintVars;
            visitDepthFirst(*clause, [&](const AstConstraint& cons) {
                visitDepthFirst(cons, [&](const AstVariable& var) { ignoredVars.insert(var.getName()); });
            });

            std::set<std::string> singletons;
            for (auto& var : vars) {
                if ((nonsingletons.find(var) == nonsingletons.end()) &&
                        (ignoredVars.find(var) == ignoredVars.end())) {
                    changed = true;
                    singletons.insert(var);
                }
            }

            // Replace the singletons found with underscores
            replaceSingletons update(singletons);
            clause->apply(update);
        }
    }

    return changed;
}

}  // end of namespace souffle
