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
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Utils.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace souffle::ram::transform {

bool CollapseFiltersTransformer::collapseFilters(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const Filter* filter = as<Filter>(node)) {
            // true if two consecutive filters in loop nest found
            bool canCollapse = false;

            // storing conditions for collapsing
            VecOwn<Condition> conditions;

            const Filter* prevFilter = filter;
            conditions.emplace_back(filter->getCondition().cloning());
            while (auto* nextFilter = as<Filter>(prevFilter->getOperation())) {
                canCollapse = true;
                conditions.emplace_back(nextFilter->getCondition().cloning());
                prevFilter = nextFilter;
            }

            if (canCollapse) {
                changed = true;
                node = mk<Filter>(toCondition(conditions), clone(prevFilter->getOperation()),
                        prevFilter->getProfileText());
            }
        }

        node->apply(go);
        return node;
    });
    return changed;
}

}  // namespace souffle::ram::transform
