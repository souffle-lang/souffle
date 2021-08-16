/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Relation.h
 *
 * Analysis that looks up a relation by name.
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/TranslationUnit.h"

namespace souffle::ram {

class Relation;

namespace analysis {

/**
 * @class RelationAnalysis
 * @brief A RAM Analysis for finding relations by name
 *
 */
class RelationAnalysis : public Analysis {
public:
    RelationAnalysis() : Analysis(name) {}

    static constexpr const char* name = "relation-analysis";

    void run(const TranslationUnit&) override;

    const ram::Relation& lookup(const std::string& name) const;

protected:
    std::map<std::string, const ram::Relation*> relationMap;
};

}  // namespace analysis
}  // namespace souffle::ram
