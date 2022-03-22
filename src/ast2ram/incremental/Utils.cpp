/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.cpp
 *
 * A collection of utilities used in translation
 *
 ***********************************************************************/

#include "ast2ram/incremental/Utils.h"
#include "ast/QualifiedName.h"

#include "ast2ram/utility/Location.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StringUtil.h"
#include <string>
#include <vector>

namespace souffle::ast2ram::incremental {

std::string getDiffPlusRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@diff_plus_");
}

std::string getDiffMinusRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@diff_minus_");
}

std::string getActualDiffPlusRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@actual_diff_plus_");
}

std::string getActualDiffMinusRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@actual_diff_minus_");
}

std::string getNewDiffPlusRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_diff_plus_");
}

std::string getNewDiffMinusRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_diff_minus_");
}

std::string getPrevRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@prev_");
}

std::string getNewPrevRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_prev_");
}

}  // namespace souffle::ast2ram::incremental
