/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.h
 *
 * A collection of utilities used in translation
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/ContainerUtil.h"
#include "ast2ram/utility/Utils.h"
#include <string>

namespace souffle::ast {
class QualifiedName;
}  // namespace souffle::ast

namespace souffle::ast2ram::incremental {

/** Get the corresponding diff_plus relation name, used for incremental inserts */
std::string getDiffPlusRelationName(const ast::QualifiedName& name);

/** Get the corresponding diff_minus relation name, used for incremental deletes */
std::string getDiffMinusRelationName(const ast::QualifiedName& name);

/** Get actual versions of the above, since diff_plus/diff_minus may be over-approximations */
std::string getActualDiffPlusRelationName(const ast::QualifiedName& name);
std::string getActualDiffMinusRelationName(const ast::QualifiedName& name);

/** Get new versions of the above for recursive relations */
std::string getNewDiffPlusRelationName(const ast::QualifiedName& name);
std::string getNewDiffMinusRelationName(const ast::QualifiedName& name);

/** Get the corresponding prev relation name, storing the state of the relation before an incremental update */
std::string getPrevRelationName(const ast::QualifiedName& name);
// std::string getDeltaPrevRelationName(const ast::QualifiedName& name);
std::string getNewPrevRelationName(const ast::QualifiedName& name);

}  // namespace souffle::ast2ram::incremental
