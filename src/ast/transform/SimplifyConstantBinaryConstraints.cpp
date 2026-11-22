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
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/typesystem/PolymorphicObjects.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/StringUtil.h"
#include <optional>

namespace souffle::ast::transform {

static bool equalNumericConstants(
        const NumericConstant& lhs, const NumericConstant& rhs, NumericConstant::Type type) {
    const auto& left = lhs.getConstant();
    const auto& right = rhs.getConstant();
    switch (type) {
        case NumericConstant::Type::Int:
            return RamSignedFromString(left, nullptr, 0) == RamSignedFromString(right, nullptr, 0);
        case NumericConstant::Type::Uint:
            return RamUnsignedFromString(left, nullptr, 0) == RamUnsignedFromString(right, nullptr, 0);
        case NumericConstant::Type::Float: return RamFloatFromString(left) == RamFloatFromString(right);
    }
    fatal("unexpected numeric constant type");
}

bool SimplifyConstantBinaryConstraintsTransformer::transform(TranslationUnit& translationUnit) {
    Program& program = translationUnit.getProgram();
    bool changed = false;
    const analysis::PolymorphicObjectsAnalysis* types = nullptr;
    auto foldConstraint = [&](const Literal& literal) -> std::optional<bool> {
        const auto* constraint = as<BinaryConstraint>(&literal);
        if (constraint == nullptr) {
            return std::nullopt;
        }
        const auto op = constraint->getBaseOperator();
        if (op != BinaryConstraintOp::EQ && op != BinaryConstraintOp::NE) {
            return std::nullopt;
        }
        const auto* lhs = as<Constant>(constraint->getLHS());
        const auto* rhs = as<Constant>(constraint->getRHS());
        if (lhs == nullptr || rhs == nullptr) {
            return std::nullopt;
        }

        const auto* left = as<NumericConstant>(lhs);
        const auto* right = as<NumericConstant>(rhs);
        if (left == nullptr && right == nullptr) {
            return (*lhs == *rhs) == (op == BinaryConstraintOp::EQ);
        }
        if (left == nullptr || right == nullptr) {
            return std::nullopt;
        }
        if (types == nullptr) {
            types = &translationUnit.getAnalysis<analysis::PolymorphicObjectsAnalysis>();
        }
        if (types->hasInvalidType(*left) || types->hasInvalidType(*right)) {
            return std::nullopt;
        }
        const auto type = types->getInferredType(*left);
        if (type != types->getInferredType(*right)) {
            return std::nullopt;
        }
        // AST equality also checks the explicit type annotation and the literal spelling.
        return equalNumericConstants(*left, *right, type) == (op == BinaryConstraintOp::EQ);
    };

    for (Relation* rel : program.getRelations()) {
        for (auto&& clause : program.getClauses(*rel)) {
            const auto body = clause->getBodyLiterals();
            Own<Clause> replacementClause;
            for (auto it = body.begin(); it != body.end(); ++it) {
                const auto value = foldConstraint(**it);
                if (value.has_value()) {
                    if (!replacementClause) {
                        replacementClause = Own<Clause>(clause->cloneHead());
                        for (auto previous = body.begin(); previous != it; ++previous) {
                            replacementClause->addToBody(clone(*previous));
                        }
                    }
                    replacementClause->addToBody(mk<BooleanConstraint>(*value));
                } else if (replacementClause) {
                    replacementClause->addToBody(clone(*it));
                }
            }
            if (!replacementClause) {
                continue;
            }

            program.removeClause(*clause);
            program.addClause(std::move(replacementClause));
            changed = true;
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
