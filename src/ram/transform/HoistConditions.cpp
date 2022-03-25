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
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

bool HoistConditionsTransformer::hoistConditions(Program& program) {
    bool changed = false;

    // helper for collecting conditions from filter operations
    auto addCondition = [](Own<Condition> condition, Own<Condition> c) -> Own<Condition> {
        if (condition == nullptr) {
            return c;
        } else {
            return mk<Conjunction>(std::move(condition), std::move(c));
        }
    };

    // hoist conditions to the most outer scope if they
    // don't depend on TupleOperations
    forEachQuery(program, [&](Query& query) {
        Own<Condition> newCondition;
        query.apply(nodeMapper<Node>([&](auto&& go, Own<Node> node) -> Own<Node> {
            if (auto* filter = as<Filter>(node)) {
                const Condition& condition = filter->getCondition();
                // if filter condition is independent of any TupleOperation,
                // delete the filter operation and collect condition
                if (!rla->hasLevel(&condition)) {
                    changed = true;
                    newCondition = addCondition(std::move(newCondition), clone(condition));
                    node->apply(go);
                    return clone(filter->getOperation());
                }
            }

            node->apply(go);
            return node;
        }));

        if (newCondition != nullptr) {
            // insert new filter operation at outer-most level of the query
            changed = true;
            auto* nestedOp = &query.getOperation();
            query.rewrite(nestedOp, mk<Filter>(std::move(newCondition), clone(nestedOp)));
        }
    });

    // hoist conditions for each TupleOperation operation
    visit(program, [&](TupleOperation& search) {
        Own<Condition> newCondition;
        search.apply(nodeMapper<Node>([&](auto&& go, Own<Node> node) -> Own<Node> {
            if (auto* filter = as<Filter>(node)) {
                const Condition& condition = filter->getCondition();
                // if filter condition matches level of TupleOperation,
                // delete the filter operation and collect condition
                if (rla->getLevel(&condition) == search.getTupleId()) {
                    changed = true;
                    newCondition = addCondition(std::move(newCondition), clone(condition));
                    node->apply(go);
                    return clone(filter->getOperation());
                }
            }

            node->apply(go);
            return node;
        }));

        if (newCondition != nullptr) {
            // insert new filter operation after the search operation
            changed = true;
            search.rewrite(&search.getOperation(),
                    mk<Filter>(std::move(newCondition), clone(search.getOperation())));
        }
    });
    return changed;
}

}  // namespace souffle::ram::transform
