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
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

bool HoistAggregateTransformer::hoistAggregate(Program& program) {
    bool changed = false;

    // There are two cases: aggregates that have no data-dependencies on
    // other RAM operations, and aggregates that have data-dependencies.
    // A rewriter has two tasks: (1) identify a single aggregate that
    // can be hoisted and (2) insert it at the outermost level.
    // We assume all Operations are renumbered for this transformation.

    // Hoist a single aggregate to an outer scope that is data-independent.
    visit(program, [&](const Query& query) {
        Own<NestedOperation> newAgg;
        bool priorTupleOp = false;
        std::function<Own<Node>(Own<Node>)> aggRewriter = [&](Own<Node> node) -> Own<Node> {
            if (isA<Aggregate>(node)) {
                auto* tupleOp = as<TupleOperation>(node);
                assert(tupleOp != nullptr && "aggregate conversion to tuple operation failed");
                if (rla->getLevel(tupleOp) == -1 && !priorTupleOp) {
                    changed = true;
                    newAgg = clone(tupleOp);
                    assert(newAgg != nullptr && "failed to make a cloning");
                    return clone(tupleOp->getOperation());
                }
            } else if (isA<TupleOperation>(node)) {
                // tuple operation that is a non-aggregate
                priorTupleOp = true;
            }
            node->apply(makeLambdaRamMapper(aggRewriter));
            return node;
        };
        const_cast<Query*>(&query)->apply(makeLambdaRamMapper(aggRewriter));
        if (newAgg != nullptr) {
            newAgg->rewrite(&newAgg->getOperation(), clone(query.getOperation()));
            const_cast<Query*>(&query)->rewrite(&query.getOperation(), std::move(newAgg));
        }
    });

    // hoist a single aggregate to an outer scope that is data-dependent on a prior operation.
    visit(program, [&](const Query& query) {
        int newLevel = -1;
        Own<NestedOperation> newAgg;
        int priorOpLevel = -1;

        std::function<Own<Node>(Own<Node>)> aggRewriter = [&](Own<Node> node) -> Own<Node> {
            if (as<AbstractAggregate, AllowCrossCast>(node)) {
                auto* tupleOp = as<TupleOperation>(node);
                assert(tupleOp != nullptr && "aggregate conversion to nested operation failed");
                int dataDepLevel = rla->getLevel(tupleOp);
                if (dataDepLevel != -1 && dataDepLevel < tupleOp->getTupleId() - 1) {
                    // If all tuple ops between the data-dependence level and agg
                    // are aggregates, then we do not hoist, i.e., we would
                    // continuously swap their positions.
                    if (dataDepLevel != priorOpLevel) {
                        changed = true;
                        newLevel = dataDepLevel;
                        newAgg = clone(tupleOp);
                        assert(newAgg != nullptr && "failed to make a cloning");
                        return clone(tupleOp->getOperation());
                    }
                }
            } else if (const TupleOperation* tupleOp = as<TupleOperation>(node)) {
                priorOpLevel = tupleOp->getTupleId();
            }
            node->apply(makeLambdaRamMapper(aggRewriter));
            if (auto* search = as<TupleOperation>(node)) {
                if (newAgg != nullptr && search->getTupleId() == newLevel) {
                    newAgg->rewrite(&newAgg->getOperation(), clone(search->getOperation()));
                    search->rewrite(&search->getOperation(), std::move(newAgg));
                }
            }
            return node;
        };
        const_cast<Query*>(&query)->apply(makeLambdaRamMapper(aggRewriter));
    });
    return changed;
}  // namespace souffle

}  // namespace souffle::ram::transform
