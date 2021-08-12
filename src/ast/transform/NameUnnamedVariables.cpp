/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
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
#include "ast/Clause.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include <cstddef>
#include <memory>
#include <ostream>
#include <vector>

namespace souffle::ast::transform {

bool NameUnnamedVariablesTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    static constexpr const char* boundPrefix = "+underscore";
    static std::size_t underscoreCount = 0;

    struct nameVariables : public NodeMapper {
        mutable bool changed = false;
        nameVariables() = default;

        Own<Node> operator()(Own<Node> node) const override {
            if (isA<Negation>(node)) {
                return node;
            }
            if (isA<UnnamedVariable>(node)) {
                changed = true;
                std::stringstream name;
                name << boundPrefix << "_" << underscoreCount++;
                return mk<ast::Variable>(name.str());
            }
            node->apply(*this);
            return node;
        }
    };

    Program& program = translationUnit.getProgram();
    for (Relation* rel : program.getRelations()) {
        for (Clause* clause : getClauses(program, *rel)) {
            nameVariables update;
            clause->apply(update);
            changed |= update.changed;
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
