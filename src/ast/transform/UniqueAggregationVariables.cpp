/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UniqueAggregationVariables.cpp
 *
 ***********************************************************************/

#include "ast/transform/UniqueAggregationVariables.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstVisitor.h"
#include "souffle/utility/StringUtil.h"
#include <set>
#include <vector>

namespace souffle {

bool UniqueAggregationVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    // make variables in aggregates unique
    int aggNumber = 0;
    visitDepthFirstPostOrder(*translationUnit.getProgram(), [&](const AstAggregator& agg) {
        // only applicable for aggregates with target expression
        if (agg.getTargetExpression() == nullptr) {
            return;
        }

        // get all variables in the target expression
        std::set<std::string> names;
        visitDepthFirst(
                *agg.getTargetExpression(), [&](const AstVariable& var) { names.insert(var.getName()); });

        // rename them
        visitDepthFirst(agg, [&](const AstVariable& var) {
            auto pos = names.find(var.getName());
            if (pos == names.end()) {
                return;
            }
            const_cast<AstVariable&>(var).setName(" " + var.getName() + toString(aggNumber));
            changed = true;
        });

        // increment aggregation number
        aggNumber++;
    });
    return changed;
}

}  // end of namespace souffle
