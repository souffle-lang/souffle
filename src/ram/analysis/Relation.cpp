/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Relation.cpp
 *
 * Implementation of RAM Relation Analysis
 *
 ***********************************************************************/

#include "ram/Relation.h"
#include "ram/analysis/Relation.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace souffle::ram::analysis {

const ram::Relation& RelationAnalysis::lookup(const std::string& name) const {
    auto it = relationMap.find(name);
    assert(it != relationMap.end() && "relation not found");
    return *(it->second);
}

void RelationAnalysis::run(const TranslationUnit& translationUnit) {
    visit(translationUnit.getProgram(),
            [&](const Relation& relation) { relationMap[relation.getName()] = &relation; });
}

}  // namespace souffle::ram::analysis
