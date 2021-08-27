/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SumTypeBranches.cpp
 *
 * Calculate branch to types mapping.
 *
 ***********************************************************************/

#include "ast/analysis/SumTypeBranches.h"
#include "ast/AlgebraicDataType.h"
#include "ast/BranchType.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "ast/analysis/TypeEnvironment.h"
#include "ast/analysis/TypeSystem.h"
#include "ast/utility/Visitor.h"
#include <vector>

namespace souffle::ast::analysis {

void SumTypeBranchesAnalysis::run(const TranslationUnit& tu) {
    const TypeEnvironment& env = tu.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    Program& program = tu.getProgram();
    visit(program.getTypes(), [&](const ast::AlgebraicDataType& adt) {
        auto typeName = adt.getQualifiedName();
        if (!env.isType(typeName)) return;

        for (auto& branch : adt.getBranches()) {
            branchToType[branch->getBranchName()] = &env.getType(typeName);
        }
    });
}

}  // namespace souffle::ast::analysis
