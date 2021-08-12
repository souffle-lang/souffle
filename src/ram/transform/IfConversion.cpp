/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IfConversion.cpp
 *
 ***********************************************************************/

#include "ram/transform/IfConversion.h"
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
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace souffle::ram::transform {

Own<Operation> IfConversionTransformer::rewriteIndexScan(const IndexScan* indexScan) {
    // check whether tuple is used in subsequent operations
    bool tupleUsed = visitExists(*indexScan,
            [&](const TupleElement& element) { return element.getTupleId() == indexScan->getTupleId(); });

    // if not used, transform the IndexScan operation to an existence check
    if (!tupleUsed) {
        // existence check is only supported for equality predicates on each attribute
        std::size_t arity = indexScan->getRangePattern().first.size();
        for (std::size_t i = 0; i < arity; ++i) {
            if (*(indexScan->getRangePattern().first[i]) != *(indexScan->getRangePattern().second[i])) {
                return nullptr;
            }
        }
        // replace IndexScan with an Filter/Existence check
        RamBound newValues = clone(indexScan->getRangePattern().first);

        // check if there is a break statement nested in the Scan - if so, remove it
        Own<Operation> newOp;
        if (auto* breakOp = as<Break>(indexScan->getOperation())) {
            newOp = clone(breakOp->getOperation());
        } else {
            newOp = clone(indexScan->getOperation());
        }

        return mk<Filter>(mk<ExistenceCheck>(indexScan->getRelation(), std::move(newValues)),
                std::move(newOp), indexScan->getProfileText());
    }

    return nullptr;
}

bool IfConversionTransformer::convertIndexScans(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const IndexScan* scan = as<IndexScan>(node)) {
            if (Own<Operation> op = rewriteIndexScan(scan)) {
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
