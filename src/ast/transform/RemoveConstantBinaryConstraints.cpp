/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveConstantBinaryConstraints.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveConstantBinaryConstraints.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "souffle/BinaryConstraintOps.h"

namespace souffle::ast::transform {

bool RemoveConstantBinaryConstraintsTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    bool changed = false;

    for (Relation* rel : program.getRelations()) {
        for (auto&& clause : program.getClauses(*rel)) {
            bool infeasible = false;

            for (Literal* lit : clause->getBodyLiterals()) {
                if (auto* bc = as<BinaryConstraint>(lit)) {
                    auto* lhs = bc->getLHS();
                    auto* rhs = bc->getRHS();
                    Constant *lhsConst, *rhsConst;
                    if ((lhsConst = as<Constant>(lhs)) && (rhsConst = as<Constant>(rhs))) {
                        if ((bc->getBaseOperator() == BinaryConstraintOp::EQ && !(*lhsConst == *rhsConst)) ||
                                (bc->getBaseOperator() == BinaryConstraintOp::NE &&
                                        (*lhsConst == *rhsConst))) {
                            infeasible = true;
                            break;
                        }
                    }
                }
            }

            if (infeasible) {
                // Clause will always fail
                changed = true;
                program.removeClause(*clause);
            }
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
