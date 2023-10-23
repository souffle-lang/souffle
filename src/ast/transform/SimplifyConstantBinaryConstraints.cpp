/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SimplifyConstantBinaryConstraints.cpp
 *
 ***********************************************************************/

#include "ast/transform/SimplifyConstantBinaryConstraints.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "souffle/BinaryConstraintOps.h"

namespace souffle::ast::transform {

bool SimplifyConstantBinaryConstraintsTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    bool changed = false;

    for (Relation* rel : program.getRelations()) {
        for (auto&& clause : program.getClauses(*rel)) {
            bool canSimplify = false;

            for (Literal* lit : clause->getBodyLiterals()) {
                if (auto* bc = as<BinaryConstraint>(lit)) {
                    if (isA<Constant>(*bc->getLHS()) && isA<Constant>(*bc->getRHS()) &&
                            (bc->getBaseOperator() == BinaryConstraintOp::EQ ||
                                    bc->getBaseOperator() == BinaryConstraintOp::NE)) {
                        canSimplify = true;
                        break;
                    }
                }
            }

            if (!canSimplify) {
                continue;
            }

            auto replacementClause = Own<Clause>(clause->cloneHead());
            for (Literal* lit : clause->getBodyLiterals()) {
                bool replaced = false;

                if (auto* bc = as<BinaryConstraint>(lit)) {
                    auto* lhs = bc->getLHS();
                    auto* rhs = bc->getRHS();
                    Constant *lhsConst, *rhsConst;
                    if ((lhsConst = as<Constant>(lhs)) && (rhsConst = as<Constant>(rhs))) {
                        bool constsEq = *lhsConst == *rhsConst;
                        if (bc->getBaseOperator() == BinaryConstraintOp::EQ ||
                                bc->getBaseOperator() == BinaryConstraintOp::NE) {
                            bool literalBool = constsEq;
                            if (bc->getBaseOperator() == BinaryConstraintOp::NE) {
                                literalBool = !constsEq;
                            }
                            BooleanConstraint replacementLiteral(literalBool);
                            replacementClause->addToBody(clone(replacementLiteral));
                            replaced = true;
                        }
                    }
                }

                if (!replaced) {
                    replacementClause->addToBody(clone(lit));
                }
            }

            program.removeClause(*clause);
            program.addClause(std::move(replacementClause));
            changed = true;
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
