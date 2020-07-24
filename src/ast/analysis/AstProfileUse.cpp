/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstProfileUse.cpp
 *
 * Implements an AstAnalysis that provides profile information
 * from a profile log file for profile-guided optimisations.
 *
 ***********************************************************************/

#include "ast/analysis/AstProfileUse.h"
#include "Global.h"
#include "ast/AstQualifiedName.h"
#include "souffle/profile/ProgramRun.h"
#include "souffle/profile/Reader.h"
#include "souffle/profile/Relation.h"
#include <limits>
#include <string>

namespace souffle {

class AstTranslationUnit;

/**
 * Run analysis, i.e., retrieve profile information
 */
void AstProfileUseAnalysis::run(const AstTranslationUnit&) {
    if (Global::config().has("profile-use")) {
        std::string filename = Global::config().get("profile-use");
        profile::Reader(filename, programRun).processFile();
    }
}

/**
 * Print analysis
 */
void AstProfileUseAnalysis::print(std::ostream&) const {}

/**
 * Check whether relation size is defined in profile
 */
bool AstProfileUseAnalysis::hasRelationSize(const AstQualifiedName& rel) {
    return programRun->getRelation(rel.toString()) != nullptr;
}

/**
 * Get relation size from profile
 */
size_t AstProfileUseAnalysis::getRelationSize(const AstQualifiedName& rel) {
    if (const auto* profRel = programRun->getRelation(rel.toString())) {
        return profRel->size();
    } else {
        return std::numeric_limits<size_t>::max();
    }
}

}  // end of namespace souffle
