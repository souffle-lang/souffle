/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IndexedInequality.cpp
 *
 ***********************************************************************/

#include "ram/transform/IndexedInequality.h"
#include "BinaryConstraintOps.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "ram/Utils.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace souffle {

bool IndexedInequalityTransformer::transformIndexToFilter(RamProgram& program) {
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

    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> indexToFilterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // find a RamIndexOperation
            if (const RamIndexOperation* indexOperation = dynamic_cast<RamIndexOperation*>(node.get())) {
                auto indexSelection = idxAnalysis->getIndexes(indexOperation->getRelation());
                auto attributesToDischarge = indexSelection.getAttributesToDischarge(
                        idxAnalysis->getSearchSignature(indexOperation), indexOperation->getRelation());

                auto pattern = indexOperation->getRangePattern();
                std::unique_ptr<RamCondition> condition;
                RamPattern updatedPattern;

                for (RamExpression* p : indexOperation->getRangePattern().first) {
                    updatedPattern.first.emplace_back(p->clone());
                }
                for (RamExpression* p : indexOperation->getRangePattern().second) {
                    updatedPattern.second.emplace_back(p->clone());
                }
                for (auto i : attributesToDischarge) {
                    // move constraints out of the indexed inequality and into a conjuction
                    std::unique_ptr<RamConstraint> lowerBound;
                    std::unique_ptr<RamConstraint> upperBound;
                    changed = true;

                    if (!isRamUndefValue(pattern.first[i])) {
                        lowerBound = std::make_unique<RamConstraint>(BinaryConstraintOp::GE,
                                std::make_unique<RamTupleElement>(indexOperation->getTupleId(), i),
                                souffle::clone(pattern.first[i]));
                        condition = addCondition(std::move(condition), souffle::clone(lowerBound));
                    }

                    if (!isRamUndefValue(pattern.second[i])) {
                        upperBound = std::make_unique<RamConstraint>(BinaryConstraintOp::LE,
                                std::make_unique<RamTupleElement>(indexOperation->getTupleId(), i),
                                souffle::clone(pattern.second[i]));
                        condition = addCondition(std::move(condition), souffle::clone(upperBound));
                    }

                    // reset the bounds
                    updatedPattern.first[i] = std::make_unique<RamUndefValue>();
                    updatedPattern.second[i] = std::make_unique<RamUndefValue>();
                }

                if (condition) {
                    auto nestedOp = souffle::clone(&indexOperation->getOperation());
                    auto filter = std::make_unique<RamFilter>(std::move(condition), std::move(nestedOp));

                    // need to rewrite the node with the same index operation
                    if (const RamIndexScan* iscan = dynamic_cast<RamIndexScan*>(node.get())) {
                        node = std::make_unique<RamIndexScan>(
                                std::make_unique<RamRelationReference>(&iscan->getRelation()),
                                iscan->getTupleId(), std::move(updatedPattern), std::move(filter),
                                iscan->getProfileText());
                    } else if (const RamParallelIndexScan* pscan =
                                       dynamic_cast<RamParallelIndexScan*>(node.get())) {
                        node = std::make_unique<RamParallelIndexScan>(
                                std::make_unique<RamRelationReference>(&pscan->getRelation()),
                                pscan->getTupleId(), std::move(updatedPattern), std::move(filter),
                                pscan->getProfileText());
                    } else if (const RamIndexChoice* ichoice = dynamic_cast<RamIndexChoice*>(node.get())) {
                        node = std::make_unique<RamIndexChoice>(
                                std::make_unique<RamRelationReference>(&ichoice->getRelation()),
                                ichoice->getTupleId(), souffle::clone(&ichoice->getCondition()),
                                std::move(updatedPattern), std::move(filter), ichoice->getProfileText());
                    } else if (const RamIndexAggregate* iagg = dynamic_cast<RamIndexAggregate*>(node.get())) {
                        node = std::make_unique<RamIndexAggregate>(std::move(filter), iagg->getFunction(),
                                std::make_unique<RamRelationReference>(&iagg->getRelation()),
                                souffle::clone(&iagg->getExpression()), souffle::clone(&iagg->getCondition()),
                                std::move(updatedPattern), iagg->getTupleId());
                    } else {
                        fatal("New RamIndexOperation subclass found but not supported while making index.");
                    }
                }
            }
            node->apply(makeLambdaRamMapper(indexToFilterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(indexToFilterRewriter));
    });
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> removeEmptyIndexRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // find an IndexOperation
            if (const RamIndexOperation* indexOperation = dynamic_cast<RamIndexOperation*>(node.get())) {
                auto pattern = indexOperation->getRangePattern();
                size_t length = pattern.first.size();
                bool foundRealIndexableOperation = false;

                for (size_t i = 0; i < length; ++i) {
                    // if both bounds are undefined we don't have a box query
                    if (isRamUndefValue(pattern.first[i]) && isRamUndefValue(pattern.second[i])) {
                        continue;
                    }
                    // if lower and upper bounds are equal its also not a box query
                    foundRealIndexableOperation = true;
                    break;
                }
                if (!foundRealIndexableOperation) {
                    // need to rewrite the node with a semantically equivalent operation to get rid of the
                    // index operation i.e. RamIndexScan with no indexable attributes -> RamScan
                    if (const RamIndexScan* iscan = dynamic_cast<RamIndexScan*>(node.get())) {
                        node = std::make_unique<RamScan>(
                                std::make_unique<RamRelationReference>(&iscan->getRelation()),
                                iscan->getTupleId(), souffle::clone(&iscan->getOperation()),
                                iscan->getProfileText());
                    } else if (const RamParallelIndexScan* pscan =
                                       dynamic_cast<RamParallelIndexScan*>(node.get())) {
                        node = std::make_unique<RamParallelScan>(
                                std::make_unique<RamRelationReference>(&pscan->getRelation()),
                                pscan->getTupleId(), souffle::clone(&pscan->getOperation()),
                                pscan->getProfileText());
                    } else if (const RamIndexChoice* ichoice = dynamic_cast<RamIndexChoice*>(node.get())) {
                        node = std::make_unique<RamChoice>(
                                std::make_unique<RamRelationReference>(&ichoice->getRelation()),
                                ichoice->getTupleId(), souffle::clone(&ichoice->getCondition()),
                                souffle::clone(&ichoice->getOperation()), ichoice->getProfileText());
                    } else if (const RamIndexAggregate* iagg = dynamic_cast<RamIndexAggregate*>(node.get())) {
                        node = std::make_unique<RamAggregate>(souffle::clone(&iagg->getOperation()),
                                iagg->getFunction(),
                                std::make_unique<RamRelationReference>(&iagg->getRelation()),
                                souffle::clone(&iagg->getExpression()), souffle::clone(&iagg->getCondition()),
                                iagg->getTupleId());
                    } else {
                        fatal("New RamIndexOperation subclass found but not supported while transforming "
                              "index.");
                    }
                }
            }
            node->apply(makeLambdaRamMapper(removeEmptyIndexRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(removeEmptyIndexRewriter));
    });
    return changed;
}

}  // end of namespace souffle
