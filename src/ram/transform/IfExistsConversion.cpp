/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IfExistsConversion.cpp
 *
 ***********************************************************************/

#include "ram/transform/IfExistsConversion.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

Own<Operation> IfExistsConversionTransformer::rewriteScan(const Scan* scan) {
    bool transformTuple = false;

    // Check that Filter follows the Scan in the loop nest
    if (const auto* filter = as<Filter>(scan->getOperation())) {
        // Check that the Filter uses the identifier in the Scan
        if (rla->getLevel(&filter->getCondition()) == scan->getTupleId()) {
            transformTuple = true;

            const Node& nextNode = filter->getOperation();

            visit(nextNode, [&](const TupleElement& element) {
                if (element.getTupleId() == scan->getTupleId()) {
                    transformTuple = false;
                }
            });
        }
    }

    // Convert the Scan/If pair into a IfExists
    if (transformTuple) {
        const auto* filter = as<Filter>(scan->getOperation());
        const std::size_t identifier = scan->getTupleId();

        return mk<IfExists>(scan->getRelation(), identifier, clone(filter->getCondition()),
                clone(filter->getOperation()), scan->getProfileText());
    }

    // Check that Relation is not referenced further down in the loop nest
    bool referencedBelow = false;
    visit(*scan, [&](const TupleElement& element) {
        if (element.getTupleId() == scan->getTupleId()) {
            referencedBelow = true;
        }
    });

    // Convert the Scan into a IfExists where True
    if (!referencedBelow) {
        return mk<IfExists>(scan->getRelation(), scan->getTupleId(), mk<True>(), clone(scan->getOperation()),
                scan->getProfileText());
    }

    return nullptr;
}

Own<Operation> IfExistsConversionTransformer::rewriteIndexScan(const IndexScan* indexScan) {
    bool transformTuple = false;

    // Check that Filter follows the IndexScan in the loop nest
    if (const auto* filter = as<Filter>(indexScan->getOperation())) {
        // Check that the Filter uses the identifier in the IndexScan
        if (rla->getLevel(&filter->getCondition()) == indexScan->getTupleId()) {
            transformTuple = true;

            // Check that the filter is not referred to after
            const Node& nextNode = filter->getOperation();

            visit(nextNode, [&](const TupleElement& element) {
                if (element.getTupleId() == indexScan->getTupleId()) {
                    transformTuple = false;
                }
            });
        }
    }

    // Convert the IndexScan/If pair into an IndexIfExists
    if (transformTuple) {
        RamPattern newValues = make_pair(
                clone(indexScan->getRangePattern().first), clone(indexScan->getRangePattern().second));
        const auto* filter = as<Filter>(indexScan->getOperation());
        const std::size_t identifier = indexScan->getTupleId();
        const std::string& rel = indexScan->getRelation();
        return mk<IndexIfExists>(rel, identifier, clone(filter->getCondition()), std::move(newValues),
                clone(filter->getOperation()), indexScan->getProfileText());
    }

    // Check that Relation is not referenced further down in the loop nest
    bool referencedBelow = false;
    visit(*indexScan, [&](const TupleElement& element) {
        if (element.getTupleId() == indexScan->getTupleId()) {
            referencedBelow = true;
        }
    });

    // Convert the Scan into a IfExists where True
    if (!referencedBelow) {
        RamPattern newValues = make_pair(
                clone(indexScan->getRangePattern().first), clone(indexScan->getRangePattern().second));
        return mk<IndexIfExists>(indexScan->getRelation(), indexScan->getTupleId(), mk<True>(),
                std::move(newValues), clone(indexScan->getOperation()), indexScan->getProfileText());
    }

    return nullptr;
}

bool IfExistsConversionTransformer::convertScans(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const Scan* scan = as<Scan>(node)) {
            if (auto op = rewriteScan(scan)) {
                changed = true;
                node = std::move(op);
            }
        } else if (const IndexScan* indexScan = as<IndexScan>(node)) {
            if (auto op = rewriteIndexScan(indexScan)) {
                changed = true;
                node = std::move(op);
            }
        }

        node->apply(go);
        return node;
    });

    return changed;
}

}  // namespace souffle::ram::transform
