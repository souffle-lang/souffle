/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReplaceSingletonVariables.cpp
 *
 ***********************************************************************/

#include "ast/transform/ReplaceSingletonVariables.h"
#include "ast/BranchInit.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include <memory>
#include <set>
#include <vector>

namespace souffle::ast::transform {

bool ReplaceSingletonVariablesTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;

    Program& program = translationUnit.getProgram();

    // Node-mapper to replace a set of singletons with unnamed variables
    struct replaceSingletons : public NodeMapper {
        std::set<std::string>& singletons;

        replaceSingletons(std::set<std::string>& singletons) : singletons(singletons) {}

        Own<Node> operator()(Own<Node> node) const override {
            if (auto* var = as<ast::Variable>(node)) {
                if (singletons.find(var->getName()) != singletons.end()) {
                    return mk<UnnamedVariable>();
                }
            }
            node->apply(*this);
            return node;
        }
    };

    for (Relation* rel : program.getRelations()) {
        for (Clause* clause : getClauses(program, *rel)) {
            std::set<std::string> nonsingletons;
            std::set<std::string> vars;

            visit(*clause, [&](const ast::Variable& var) {
                const std::string& name = var.getName();
                if (vars.find(name) != vars.end()) {
                    // Variable seen before, so not a singleton variable
                    nonsingletons.insert(name);
                } else {
                    vars.insert(name);
                }
            });

            std::set<std::string> ignoredVars;

            // Don't unname singleton variables occurring in records, ADTs, or constraints.
            visitFrontier(*clause, [&](const Node& n) {
                if (as<RecordInit>(n) || as<BranchInit>(n) || as<Constraint>(n)) {
                    visit(n, [&](const Variable& var) { ignoredVars.insert(var.getName()); });
                    return true;
                }

                return false;
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

}  // namespace souffle::ast::transform
