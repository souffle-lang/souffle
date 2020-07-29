/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NameUnnamedVariables.cpp
 *
 ***********************************************************************/

#include "ast/transform/NameUnnamedVariables.h"
#include "ast/Argument.h"
#include "ast/Clause.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/Utils.h"
#include <cstddef>
#include <memory>
#include <ostream>
#include <vector>

namespace souffle {
class AstRelation;

bool NameUnnamedVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    static constexpr const char* boundPrefix = "+underscore";
    static size_t underscoreCount = 0;

    struct nameVariables : public AstNodeMapper {
        mutable bool changed = false;
        nameVariables() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (dynamic_cast<AstUnnamedVariable*>(node.get()) != nullptr) {
                changed = true;
                std::stringstream name;
                name << boundPrefix << "_" << underscoreCount++;
                return std::make_unique<AstVariable>(name.str());
            }
            node->apply(*this);
            return node;
        }
    };

    AstProgram& program = *translationUnit.getProgram();
    for (AstRelation* rel : program.getRelations()) {
        for (AstClause* clause : getClauses(program, *rel)) {
            nameVariables update;
            clause->apply(update);
            changed |= update.changed;
        }
    }

    return changed;
}

}  // end of namespace souffle
