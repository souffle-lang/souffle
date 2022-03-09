/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
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
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/TranslationUnit.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

bool RemoveRedundantSumsTransformer::transform(TranslationUnit& translationUnit) {
    struct ReplaceSumWithCount : public NodeMapper {
        ReplaceSumWithCount() = default;

        Own<Node> operator()(Own<Node> node) const override {
            // Apply to all aggregates of the form
            // sum k : { .. } where k is a constant
            if (auto* agg = as<Aggregator>(node)) {
                if (agg->getBaseOperator() == AggregateOp::SUM) {
                    if (const auto* constant = as<NumericConstant>(agg->getTargetExpression())) {
                        changed = true;
                        // Then construct the new thing to replace it with
                        auto count = mk<Aggregator>(AggregateOp::COUNT);
                        // Duplicate the body of the aggregate
                        VecOwn<Literal> newBody;
                        for (const auto& lit : agg->getBodyLiterals()) {
                            newBody.push_back(clone(lit));
                        }
                        count->setBodyLiterals(std::move(newBody));
                        auto number = clone(constant);
                        // Now it's constant * count : { ... }
                        auto result = mk<IntrinsicFunctor>("*", std::move(number), std::move(count));

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
    Program& program = translationUnit.getProgram();
    program.apply(update);
    return update.changed;
}

}  // namespace souffle::ast::transform
