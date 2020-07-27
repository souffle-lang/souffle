/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AuxArityAnalysis.cpp
 *
 * Implementation of AST analyses classes
 *
 ***********************************************************************/

#include "ast/analysis/AuxArity.h"
#include "Global.h"
#include <string>

namespace souffle {

size_t AuxiliaryArity::computeArity(const AstRelation* /* relation */) const {
    if (Global::config().has("provenance")) {
        return 2;
    } else {
        return 0;
    }
}

}  // end of namespace souffle
