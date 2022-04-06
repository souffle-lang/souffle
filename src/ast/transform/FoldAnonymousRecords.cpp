/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FoldAnonymousRecords.cpp
 *
 ***********************************************************************/

#include "ast/transform/FoldAnonymousRecords.h"
#include "ast/Argument.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Conjunction.h"
#include "ast/Disjunction.h"
#include "ast/Literal.h"
#include "ast/Program.h"
#include "ast/RecordInit.h"
#include "ast/TranslationUnit.h"
#include "ast/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

namespace souffle::ast::transform {

namespace {
bool isValidRecordConstraint(const Literal& literal) {
    auto constraint = as<BinaryConstraint>(literal);

    if (constraint == nullptr) {
        return false;
    }

    const auto* left = constraint->getLHS();
    const auto* right = constraint->getRHS();

    const auto* leftRecord = as<RecordInit>(left);
    const auto* rightRecord = as<RecordInit>(right);

    // Check if arguments are records records.
    if ((leftRecord == nullptr) || (rightRecord == nullptr)) {
        return false;
    }

    // Check if records are of the same size.
    if (leftRecord->getChildNodes().size() != rightRecord->getChildNodes().size()) {
        return false;
    }

    // Check if operator is "=" or "!="
    auto op = constraint->getBaseOperator();

    return isEqConstraint(op) || isEqConstraint(negatedConstraintOp(op));
}

/**
 * Expand constraint on records position-wise.
 *
 * eg.  `[1, 2, 3]  = [a, b, c]` => `1  = a /\ 2  = b /\ 3  = c`
 *      `[x, y, z] != [a, b, c]` => `x != a \/ x != b \/ z != c`
 *
 * Procedure assumes that argument has a valid operation,
 * that children are of type RecordInit and that the size
 * of both sides is the same
 */
Own<Literal> expandRecordConstraint(const BinaryConstraint& constraint) {
    const auto* left = as<RecordInit>(constraint.getLHS());
    const auto* right = as<RecordInit>(constraint.getRHS());

    auto leftChildren = left->getArguments();
    auto rightChildren = right->getArguments();

    assert(leftChildren.size() == rightChildren.size());

    Own<Literal> result = mk<BooleanConstraint>(isEqConstraint(constraint.getBaseOperator()), constraint.getSrcLoc());

    // [a, b..] = [c, d...] → a = c, b = d ...
    for (std::size_t i = 0; i < leftChildren.size(); ++i) {
        auto newConstraint = mk<BinaryConstraint>(
                constraint.getBaseOperator(), clone(leftChildren[i]), clone(rightChildren[i]));
        if (isEqConstraint(constraint.getBaseOperator())) {
            result = mk<Conjunction>(std::move(result), std::move(newConstraint));
        } else {
            result = mk<Disjunction>(std::move(result), std::move(newConstraint));
        }
    }

    return result;
}
}  // namespace

bool FoldAnonymousRecords::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    bool changed = false;
    visit(program, [&](const Literal& literal) {
        if (isValidRecordConstraint(literal)) changed = true;
    });

    // Remove foldable records
    struct foldRecords : public NodeMapper {
        Own<Node> operator()(Own<Node> node) const override {
            node->apply(*this);
            if (auto* constraint = as<BinaryConstraint>(node)) {
                if (isValidRecordConstraint(*constraint)) {
                    return expandRecordConstraint(*constraint);
                }
            }
            return node;
        }
    };
    foldRecords update;
    program.apply(update);

    return changed;
}

}  // namespace souffle::ast::transform
