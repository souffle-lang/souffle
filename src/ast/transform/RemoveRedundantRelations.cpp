/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveRedundantRelationsTransformer.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveRedundantRelations.h"
#include "ast/AstProgram.h"
#include "ast/AstRelation.h"
#include "ast/AstTranslationUnit.h"
#include "ast/analysis/PrecedenceGraph.h"
#include <set>

namespace souffle {

bool RemoveRedundantRelationsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    auto* redundantRelationsAnalysis = translationUnit.getAnalysis<RedundantRelations>();
    const std::set<const AstRelation*>& redundantRelations =
            redundantRelationsAnalysis->getRedundantRelations();
    if (!redundantRelations.empty()) {
        for (auto rel : redundantRelations) {
            translationUnit.getProgram()->removeRelation(rel->getQualifiedName());
            changed = true;
        }
    }
    return changed;
}

}  // end of namespace souffle
