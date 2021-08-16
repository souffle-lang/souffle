/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SumTypeBranches.h
 *
 * A wrapper/cache for calculating a mapping between branches and types that declare them.
 *
 ***********************************************************************/

#pragma once

#include "ast/TranslationUnit.h"
#include "ast/analysis/TypeSystem.h"
#include "souffle/utility/ContainerUtil.h"
#include <map>
#include <string>

namespace souffle::ast {

namespace analysis {

class SumTypeBranchesAnalysis : public Analysis {
public:
    static constexpr const char* name = "sum-type-branches";

    SumTypeBranchesAnalysis() : Analysis(name) {}

    void run(const TranslationUnit& translationUnit) override;

    /**
     * A type can be nullptr in case of a malformed program.
     */
    const Type* getType(const std::string& branch) const {
        if (contains(branchToType, branch)) {
            return branchToType.at(branch);
        } else {
            return nullptr;
        }
    }

    const AlgebraicDataType& unsafeGetType(const std::string& branch) const {
        return *as<AlgebraicDataType>(branchToType.at(branch));
    }

private:
    std::map<std::string, const Type*> branchToType;
};

}  // namespace analysis
}  // namespace souffle::ast
