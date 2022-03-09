/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AddNullariesToAtomlessAggregates.cpp
 *
 ***********************************************************************/

#include "ast/transform/AddNullariesToAtomlessAggregates.h"
#include "ast/Aggregator.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool AddNullariesToAtomlessAggregatesTransformer::transform(TranslationUnit& translationUnit) {
    bool changed{false};
    Program& program = translationUnit.getProgram();
    visit(program, [&](Aggregator& agg) {
        bool seenAtom{false};
        for (const auto& literal : agg.getBodyLiterals()) {
            if (isA<Atom>(literal)) {
                seenAtom = true;
            }
        }
        if (seenAtom) {
            return;
        }
        // We will add in the Tautology atom to the body of this aggregate now
        changed = true;
        // +Tautology()
        std::string const relName = "+Tautology";

        if (program.getRelation(relName) == nullptr) {
            // +Tautology().
            auto fact = mk<Clause>(relName);
            // .decl +Tautology()
            auto tautologyRel = mk<Relation>(relName);
            program.addRelation(std::move(tautologyRel));
            program.addClause(std::move(fact));
        }

        VecOwn<Literal> newBody;
        for (const auto& lit : agg.getBodyLiterals()) {
            newBody.push_back(clone(lit));
        }
        newBody.push_back(mk<Atom>(relName));
        agg.setBodyLiterals(std::move(newBody));
    });
    return changed;
}
}  // namespace souffle::ast::transform
