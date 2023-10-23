/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugDeltaRelation.cpp
 *
 ***********************************************************************/

#include "ast/transform/DebugDeltaRelation.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/IOType.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/utility/ContainerUtil.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool DebugDeltaRelationTransformer::updateSignature(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    for (Relation* rel : program.getRelations()) {
        if (!rel->getIsDeltaDebug().has_value()) {
            continue;
        }
        const QualifiedName originalRelName = rel->getIsDeltaDebug().value();
        Relation* orig = program.getRelation(originalRelName);
        assert(orig != nullptr);

        rel->setAttributes(clone(orig->getAttributes()));
        changed = true;
        rel->addAttribute(mk<Attribute>("<iteration>", "unsigned"));
    }
    return changed;
}

}  // namespace souffle::ast::transform
