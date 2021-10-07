/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Parallel.cpp
 *
 ***********************************************************************/

#include "ram/transform/Parallel.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

bool ParallelTransformer::parallelizeOperations(Program& program) {
    bool changed = false;

    // parallelize the most outer loop only
    // most outer loops can be scan/if-exists/indexScan/indexIfExists
    forEachQuery(program, [&](Query& query) {
        // guardedInsert cannot be parallelized
        if (visitExists(query, [&](const GuardedInsert&) { return true; })) return;
        // erase cannot be parallelized
        if (visitExists(query, [&](const Erase&) { return true; })) return;

        query.apply(nodeMapper<Node>([&](auto&& go, Own<Node> node) -> Own<Node> {
            if (const Scan* scan = as<Scan>(node)) {
                const Relation& rel = relAnalysis->lookup(scan->getRelation());
                if (scan->getTupleId() == 0 && rel.getArity() > 0) {
                    if (!isA<Insert>(&scan->getOperation())) {
                        changed = true;
                        return mk<ParallelScan>(scan->getRelation(), scan->getTupleId(),
                                clone(scan->getOperation()), scan->getProfileText());
                    }
                }
            } else if (const IfExists* ifexists = as<IfExists>(node)) {
                if (ifexists->getTupleId() == 0) {
                    changed = true;
                    return mk<ParallelIfExists>(ifexists->getRelation(), ifexists->getTupleId(),
                            clone(ifexists->getCondition()), clone(ifexists->getOperation()),
                            ifexists->getProfileText());
                }
            } else if (const IndexScan* indexScan = as<IndexScan>(node)) {
                if (indexScan->getTupleId() == 0) {
                    changed = true;
                    RamPattern queryPattern = clone(indexScan->getRangePattern());
                    return mk<ParallelIndexScan>(indexScan->getRelation(), indexScan->getTupleId(),
                            std::move(queryPattern), clone(indexScan->getOperation()),
                            indexScan->getProfileText());
                }
            } else if (const IndexIfExists* indexIfExists = as<IndexIfExists>(node)) {
                if (indexIfExists->getTupleId() == 0) {
                    changed = true;
                    RamPattern queryPattern = clone(indexIfExists->getRangePattern());
                    return mk<ParallelIndexIfExists>(indexIfExists->getRelation(),
                            indexIfExists->getTupleId(), clone(indexIfExists->getCondition()),
                            std::move(queryPattern), clone(indexIfExists->getOperation()),
                            indexIfExists->getProfileText());
                }
            } else if (const Aggregate* aggregate = as<Aggregate>(node)) {
                const Relation& rel = relAnalysis->lookup(aggregate->getRelation());
                if (aggregate->getTupleId() == 0 && !rel.isNullary()) {
                    changed = true;
                    return mk<ParallelAggregate>(clone(aggregate->getOperation()), aggregate->getFunction(),
                            aggregate->getRelation(), clone(aggregate->getExpression()),
                            clone(aggregate->getCondition()), aggregate->getTupleId());
                }
            } else if (const IndexAggregate* indexAggregate = as<IndexAggregate>(node)) {
                const Relation& rel = relAnalysis->lookup(indexAggregate->getRelation());
                if (indexAggregate->getTupleId() == 0 && !rel.isNullary()) {
                    changed = true;
                    RamPattern queryPattern = clone(indexAggregate->getRangePattern());
                    return mk<ParallelIndexAggregate>(clone(indexAggregate->getOperation()),
                            indexAggregate->getFunction(), indexAggregate->getRelation(),
                            clone(indexAggregate->getExpression()), clone(indexAggregate->getCondition()),
                            std::move(queryPattern), indexAggregate->getTupleId());
                }
            }

            node->apply(go);
            return node;
        }));
    });
    return changed;
}

}  // namespace souffle::ram::transform
