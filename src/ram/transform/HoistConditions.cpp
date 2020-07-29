/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file HoistConditions.cpp
 *
 ***********************************************************************/

#include "ram/transform/HoistConditions.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {

bool HoistConditionsTransformer::hoistConditions(RamProgram& program) {
    bool changed = false;

    // helper for collecting conditions from filter operations
    auto addCondition = [](std::unique_ptr<RamCondition> condition,
                                std::unique_ptr<RamCondition> c) -> std::unique_ptr<RamCondition> {
        if (condition == nullptr) {
            return c;
        } else {
            return std::make_unique<RamConjunction>(std::move(condition), std::move(c));
        }
    };

    // hoist conditions to the most outer scope if they
    // don't depend on RamTupleOperations
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::unique_ptr<RamCondition> newCondition;
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (auto* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition& condition = filter->getCondition();
                // if filter condition is independent of any RamTupleOperation,
                // delete the filter operation and collect condition
                if (rla->getLevel(&condition) == -1) {
                    changed = true;
                    newCondition = addCondition(std::move(newCondition), souffle::clone(&condition));
                    node->apply(makeLambdaRamMapper(filterRewriter));
                    return souffle::clone(&filter->getOperation());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        auto* mQuery = const_cast<RamQuery*>(&query);
        mQuery->apply(makeLambdaRamMapper(filterRewriter));
        if (newCondition != nullptr) {
            // insert new filter operation at outer-most level of the query
            changed = true;
            auto* nestedOp = const_cast<RamOperation*>(&mQuery->getOperation());
            mQuery->rewrite(
                    nestedOp, std::make_unique<RamFilter>(std::move(newCondition), souffle::clone(nestedOp)));
        }
    });

    // hoist conditions for each RamTupleOperation operation
    visitDepthFirst(program, [&](const RamTupleOperation& search) {
        std::unique_ptr<RamCondition> newCondition;
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (auto* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition& condition = filter->getCondition();
                // if filter condition matches level of RamTupleOperation,
                // delete the filter operation and collect condition
                if (rla->getLevel(&condition) == search.getTupleId()) {
                    changed = true;
                    newCondition = addCondition(std::move(newCondition), souffle::clone(&condition));
                    node->apply(makeLambdaRamMapper(filterRewriter));
                    return souffle::clone(&filter->getOperation());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        auto* tupleOp = const_cast<RamTupleOperation*>(&search);
        tupleOp->apply(makeLambdaRamMapper(filterRewriter));
        if (newCondition != nullptr) {
            // insert new filter operation after the search operation
            changed = true;
            tupleOp->rewrite(&tupleOp->getOperation(), std::make_unique<RamFilter>(std::move(newCondition),
                                                               souffle::clone(&tupleOp->getOperation())));
        }
    });
    return changed;
}

}  // end of namespace souffle
