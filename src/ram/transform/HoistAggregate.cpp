/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file HoistAggregate.cpp
 *
 ***********************************************************************/

#include "ram/transform/HoistAggregate.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <cassert>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {

bool HoistAggregateTransformer::hoistAggregate(RamProgram& program) {
    bool changed = false;

    // There are two cases: aggregates that have no data-dependencies on
    // other RAM operations, and aggregates that have data-dependencies.
    // A rewriter has two tasks: (1) identify a single aggregate that
    // can be hoisted and (2) insert it at the outermost level.
    // We assume all RamOperations are renumbered for this transformation.

    // Hoist a single aggregate to an outer scope that is data-independent.
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::unique_ptr<RamNestedOperation> newAgg;
        bool priorTupleOp = false;
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> aggRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (nullptr != dynamic_cast<RamAggregate*>(node.get())) {
                auto* tupleOp = dynamic_cast<RamTupleOperation*>(node.get());
                assert(tupleOp != nullptr && "aggregate conversion to tuple operation failed");
                if (rla->getLevel(tupleOp) == -1 && !priorTupleOp) {
                    changed = true;
                    newAgg = souffle::clone(tupleOp);
                    assert(newAgg != nullptr && "failed to make a clone");
                    return souffle::clone(&tupleOp->getOperation());
                }
            } else if (nullptr != dynamic_cast<RamTupleOperation*>(node.get())) {
                // tuple operation that is a non-aggregate
                priorTupleOp = true;
            }
            node->apply(makeLambdaRamMapper(aggRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(aggRewriter));
        if (newAgg != nullptr) {
            newAgg->rewrite(&newAgg->getOperation(), souffle::clone(&query.getOperation()));
            const_cast<RamQuery*>(&query)->rewrite(&query.getOperation(), std::move(newAgg));
        }
    });

    // hoist a single aggregate to an outer scope that is data-dependent on a prior operation.
    visitDepthFirst(program, [&](const RamQuery& query) {
        int newLevel = -1;
        std::unique_ptr<RamNestedOperation> newAgg;
        int priorOpLevel = -1;

        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> aggRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (nullptr != dynamic_cast<RamAbstractAggregate*>(node.get())) {
                auto* tupleOp = dynamic_cast<RamTupleOperation*>(node.get());
                assert(tupleOp != nullptr && "aggregate conversion to nested operation failed");
                int dataDepLevel = rla->getLevel(tupleOp);
                if (dataDepLevel != -1 && dataDepLevel < tupleOp->getTupleId() - 1) {
                    // If all tuple ops between the data-dependence level and agg
                    // are aggregates, then we do not hoist, i.e., we would
                    // continuously swap their positions.
                    if (dataDepLevel != priorOpLevel) {
                        changed = true;
                        newLevel = dataDepLevel;
                        newAgg = souffle::clone(tupleOp);
                        assert(newAgg != nullptr && "failed to make a clone");
                        return souffle::clone(&tupleOp->getOperation());
                    }
                }
            } else if (const RamTupleOperation* tupleOp = dynamic_cast<RamTupleOperation*>(node.get())) {
                priorOpLevel = tupleOp->getTupleId();
            }
            node->apply(makeLambdaRamMapper(aggRewriter));
            if (auto* search = dynamic_cast<RamTupleOperation*>(node.get())) {
                if (newAgg != nullptr && search->getTupleId() == newLevel) {
                    newAgg->rewrite(&newAgg->getOperation(), souffle::clone(&search->getOperation()));
                    search->rewrite(&search->getOperation(), std::move(newAgg));
                }
            }
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(aggRewriter));
    });
    return changed;
}  // namespace souffle

}  // end of namespace souffle
