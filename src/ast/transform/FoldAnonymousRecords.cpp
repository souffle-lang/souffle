/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
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
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstVisitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

namespace souffle {

bool FoldAnonymousRecords::isValidRecordConstraint(const AstLiteral* literal) {
    auto constraint = dynamic_cast<const AstBinaryConstraint*>(literal);

    if (constraint == nullptr) {
        return false;
    }

    const auto* left = constraint->getLHS();
    const auto* right = constraint->getRHS();

    const auto* leftRecord = dynamic_cast<const AstRecordInit*>(left);
    const auto* rightRecord = dynamic_cast<const AstRecordInit*>(right);

    // Check if arguments are records records.
    if ((leftRecord == nullptr) || (rightRecord == nullptr)) {
        return false;
    }

    // Check if records are of the same size.
    if (leftRecord->getChildNodes().size() != rightRecord->getChildNodes().size()) {
        return false;
    }

    // Check if operator is "=" or "!="
    auto op = constraint->getOperator();

    return isEqConstraint(op) || isEqConstraint(negatedConstraintOp(op));
}

bool FoldAnonymousRecords::containsValidRecordConstraint(const AstClause& clause) {
    bool contains = false;
    visitDepthFirst(clause, [&](const AstBinaryConstraint& binary) {
        contains = (contains || isValidRecordConstraint(&binary));
    });
    return contains;
}

std::vector<std::unique_ptr<AstLiteral>> FoldAnonymousRecords::expandRecordBinaryConstraint(
        const AstBinaryConstraint& constraint) {
    std::vector<std::unique_ptr<AstLiteral>> replacedContraint;

    const auto* left = dynamic_cast<AstRecordInit*>(constraint.getLHS());
    const auto* right = dynamic_cast<AstRecordInit*>(constraint.getRHS());
    assert(left != nullptr && "Non-record passed to record method");
    assert(right != nullptr && "Non-record passed to record method");

    auto leftChildren = left->getArguments();
    auto rightChildren = right->getArguments();

    assert(leftChildren.size() == rightChildren.size());

    // [a, b..] = [c, d...] → a = c, b = d ...
    for (size_t i = 0; i < leftChildren.size(); ++i) {
        auto newConstraint = std::make_unique<AstBinaryConstraint>(
                constraint.getOperator(), souffle::clone(leftChildren[i]), souffle::clone(rightChildren[i]));
        replacedContraint.push_back(std::move(newConstraint));
    }

    // Handle edge case. Empty records.
    if (leftChildren.size() == 0) {
        if (isEqConstraint(constraint.getOperator())) {
            replacedContraint.emplace_back(new AstBooleanConstraint(true));
        } else {
            replacedContraint.emplace_back(new AstBooleanConstraint(false));
        }
    }

    return replacedContraint;
}

void FoldAnonymousRecords::transformClause(
        const AstClause& clause, std::vector<std::unique_ptr<AstClause>>& newClauses) {
    // If we have an inequality constraint, we need to create new clauses
    // At most one inequality constraint will be expanded in a single pass.
    AstBinaryConstraint* neqConstraint = nullptr;

    std::vector<std::unique_ptr<AstLiteral>> newBody;
    for (auto* literal : clause.getBodyLiterals()) {
        if (isValidRecordConstraint(literal)) {
            const AstBinaryConstraint& constraint = dynamic_cast<AstBinaryConstraint&>(*literal);

            // Simple case, [a_0, ..., a_n] = [b_0, ..., b_n]
            if (isEqConstraint(constraint.getOperator())) {
                auto transformedLiterals = expandRecordBinaryConstraint(constraint);
                std::move(std::begin(transformedLiterals), std::end(transformedLiterals),
                        std::back_inserter(newBody));

                // else if: Case [a_0, ..., a_n] != [b_0, ..., b_n].
                // track single such case, it will be expanded in the end.
            } else if (neqConstraint == nullptr) {
                neqConstraint = dynamic_cast<AstBinaryConstraint*>(literal);

                // Else: repeated inequality.
            } else {
                newBody.push_back(souffle::clone(literal));
            }

            // else, we simply copy the literal.
        } else {
            newBody.push_back(souffle::clone(literal));
        }
    }

    // If no inequality: create a single modified clause.
    if (neqConstraint == nullptr) {
        auto newClause = souffle::clone(&clause);
        newClause->setBodyLiterals(std::move(newBody));
        newClauses.emplace_back(std::move(newClause));

        // Else: For each pair in negation, we need an extra clause.
    } else {
        auto transformedLiterals = expandRecordBinaryConstraint(*neqConstraint);

        for (auto it = begin(transformedLiterals); it != end(transformedLiterals); ++it) {
            auto newClause = souffle::clone(&clause);
            auto copyBody = souffle::clone(newBody);
            copyBody.push_back(std::move(*it));

            newClause->setBodyLiterals(std::move(copyBody));

            newClauses.push_back(std::move(newClause));
        }
    }
}

bool FoldAnonymousRecords::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    AstProgram& program = *translationUnit.getProgram();

    std::vector<std::unique_ptr<AstClause>> newClauses;

    for (const auto* clause : program.getClauses()) {
        if (containsValidRecordConstraint(*clause)) {
            changed = true;
            transformClause(*clause, newClauses);
        } else {
            newClauses.emplace_back(clause->clone());
        }
    }

    // Update AstProgram.
    if (changed) {
        program.setClauses(std::move(newClauses));
    }
    return changed;
}

}  // end of namespace souffle
