/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ProfileUse.cpp
 *
 * Implements an Analysis that provides profile information
 * from a profile log file for profile-guided optimisations.
 *
 ***********************************************************************/

#include "ast/analysis/ProfileUse.h"
#include "Global.h"
#include "ast/QualifiedName.h"
#include "souffle/profile/ProgramRun.h"
#include "souffle/profile/Reader.h"
#include "souffle/profile/Relation.h"
#include <limits>
#include <string>

namespace souffle::ast::analysis {

/**
 * Run analysis, i.e., retrieve profile information
 */
void ProfileUseAnalysis::run(const TranslationUnit&) {
    std::string filename;
    if (Global::config().has("auto-schedule")) {
        filename = Global::config().get("auto-schedule");
    }
    reader = mk<profile::Reader>(filename, programRun);
    reader->processFile();
}

/**
 * Print analysis
 */
void ProfileUseAnalysis::print(std::ostream&) const {}

/**
 * Check whether relation size is defined in profile
 */
bool ProfileUseAnalysis::hasRelationSize(const QualifiedName& rel) const {
    return programRun->getRelation(rel.toString()) != nullptr;
}

/**
 * Get relation size from profile
 */
std::size_t ProfileUseAnalysis::getRelationSize(const QualifiedName& rel) const {
    if (const auto* profRel = programRun->getRelation(rel.toString())) {
        return profRel->size();
    } else {
        return std::numeric_limits<std::size_t>::max();
    }
}

bool ProfileUseAnalysis::hasAutoSchedulerStats() const {
    return reader->hasAutoSchedulerStats();
}

std::size_t ProfileUseAnalysis::getNonRecursiveUniqueKeys(
        const std::string& rel, const std::string& attributes, const std::string& constants) const {
    return reader->getNonRecursiveCountUniqueKeys(rel, attributes, constants);
}

std::size_t ProfileUseAnalysis::getRecursiveUniqueKeys(
        const std::string& rel, const std::string& attributes, const std::string& constants) const {
    return reader->getRecursiveCountUniqueKeys(rel, attributes, constants);
}

}  // namespace souffle::ast::analysis
