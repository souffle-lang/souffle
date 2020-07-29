/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ChoiceConversion.cpp
 *
 ***********************************************************************/

#include "ram/transform/ChoiceConversion.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace souffle {

std::unique_ptr<RamOperation> ChoiceConversionTransformer::rewriteScan(const RamScan* scan) {
    bool transformTuple = false;

    // Check that RamFilter follows the Scan in the loop nest
    if (const auto* filter = dynamic_cast<const RamFilter*>(&scan->getOperation())) {
        // Check that the Filter uses the identifier in the Scan
        if (rla->getLevel(&filter->getCondition()) == scan->getTupleId()) {
            transformTuple = true;

            // Check that the filter is not referred to after
            const auto* nextNode = dynamic_cast<const RamNode*>(&filter->getOperation());

            visitDepthFirst(*nextNode, [&](const RamTupleElement& element) {
                if (element.getTupleId() == scan->getTupleId()) {
                    transformTuple = false;
                }
            });
        }
    }

    // Convert the Scan/If pair into a Choice
    if (transformTuple) {
        std::vector<std::unique_ptr<RamExpression>> newValues;
        const auto* filter = dynamic_cast<const RamFilter*>(&scan->getOperation());
        const int identifier = scan->getTupleId();

        return std::make_unique<RamChoice>(std::make_unique<RamRelationReference>(&scan->getRelation()),
                identifier, souffle::clone(&filter->getCondition()), souffle::clone(&scan->getOperation()),
                scan->getProfileText());
    }
    return nullptr;
}

std::unique_ptr<RamOperation> ChoiceConversionTransformer::rewriteIndexScan(const RamIndexScan* indexScan) {
    bool transformTuple = false;

    // Check that RamFilter follows the IndexScan in the loop nest
    if (const auto* filter = dynamic_cast<const RamFilter*>(&indexScan->getOperation())) {
        // Check that the Filter uses the identifier in the IndexScan
        if (rla->getLevel(&filter->getCondition()) == indexScan->getTupleId()) {
            transformTuple = true;

            // Check that the filter is not referred to after
            const auto* nextNode = dynamic_cast<const RamNode*>(&filter->getOperation());

            visitDepthFirst(*nextNode, [&](const RamTupleElement& element) {
                if (element.getTupleId() == indexScan->getTupleId()) {
                    transformTuple = false;
                }
            });
        }
    }

    // Convert the IndexScan/If pair into an IndexChoice
    if (transformTuple) {
        RamPattern newValues;
        const auto* filter = dynamic_cast<const RamFilter*>(&indexScan->getOperation());
        const int identifier = indexScan->getTupleId();
        const RamRelation& rel = indexScan->getRelation();

        for (auto& cur : indexScan->getRangePattern().first) {
            RamExpression* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            newValues.first.emplace_back(val);
        }
        for (auto& cur : indexScan->getRangePattern().second) {
            RamExpression* val = nullptr;
            if (cur != nullptr) {
                val = cur->clone();
            }
            newValues.second.emplace_back(val);
        }

        return std::make_unique<RamIndexChoice>(std::make_unique<RamRelationReference>(&rel), identifier,
                souffle::clone(&filter->getCondition()), std::move(newValues),
                souffle::clone(&filter->getOperation()), indexScan->getProfileText());
    }
    return nullptr;
}

bool ChoiceConversionTransformer::convertScans(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> scanRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamScan* scan = dynamic_cast<RamScan*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteScan(scan)) {
                    changed = true;
                    node = std::move(op);
                }
            } else if (const RamIndexScan* indexScan = dynamic_cast<RamIndexScan*>(node.get())) {
                if (std::unique_ptr<RamOperation> op = rewriteIndexScan(indexScan)) {
                    changed = true;
                    node = std::move(op);
                }
            }
            node->apply(makeLambdaRamMapper(scanRewriter));

            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(scanRewriter));
    });

    return changed;
}

}  // end of namespace souffle
