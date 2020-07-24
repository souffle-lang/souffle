/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RemoveRedundantSums.cpp
 *
 ***********************************************************************/

#include "ast/transform/RemoveRedundantSums.h"
#include "AggregateOp.h"
#include "ast/AstAbstract.h"
#include "ast/AstArgument.h"
#include "ast/AstNode.h"
#include "ast/AstProgram.h"
#include "ast/AstTranslationUnit.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {

bool RemoveRedundantSumsTransformer::transform(AstTranslationUnit& translationUnit) {
    struct ReplaceSumWithCount : public AstNodeMapper {
        ReplaceSumWithCount() = default;

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // Apply to all aggregates of the form
            // sum k : { .. } where k is a constant
            if (auto* agg = dynamic_cast<AstAggregator*>(node.get())) {
                if (agg->getOperator() == AggregateOp::SUM) {
                    if (const auto* constant =
                                    dynamic_cast<const AstNumericConstant*>(agg->getTargetExpression())) {
                        changed = true;
                        // Then construct the new thing to replace it with
                        auto count = std::make_unique<AstAggregator>(AggregateOp::COUNT);
                        // Duplicate the body of the aggregate
                        std::vector<std::unique_ptr<AstLiteral>> newBody;
                        for (const auto& lit : agg->getBodyLiterals()) {
                            newBody.push_back(souffle::clone(lit));
                        }
                        count->setBody(std::move(newBody));
                        auto number = souffle::clone(constant);
                        // Now it's constant * count : { ... }
                        auto result = std::make_unique<AstIntrinsicFunctor>(
                                "*", std::move(number), std::move(count));

                        return result;
                    }
                }
            }
            node->apply(*this);
            return node;
        }

        // variables
        mutable bool changed = false;
    };

    ReplaceSumWithCount update;
    translationUnit.getProgram()->apply(update);
    return update.changed;
}

}  // end of namespace souffle
