/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CollapseFilters.cpp
 *
 ***********************************************************************/

#include "ram/transform/CollapseFilters.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Utils.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace souffle {

bool CollapseFiltersTransformer::collapseFilters(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                // true if two consecutive filters in loop nest found
                bool canCollapse = false;

                // storing conditions for collapsing
                std::vector<std::unique_ptr<RamCondition>> conditions;

                const RamFilter* prevFilter = filter;
                conditions.emplace_back(filter->getCondition().clone());
                while (auto* nextFilter = dynamic_cast<RamFilter*>(&prevFilter->getOperation())) {
                    canCollapse = true;
                    conditions.emplace_back(nextFilter->getCondition().clone());
                    prevFilter = nextFilter;
                }

                if (canCollapse) {
                    changed = true;
                    node = std::make_unique<RamFilter>(toCondition(conditions),
                            souffle::clone(&prevFilter->getOperation()), prevFilter->getProfileText());
                }
            }
            node->apply(makeLambdaRamMapper(filterRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(filterRewriter));
    });
    return changed;
}

}  // end of namespace souffle
