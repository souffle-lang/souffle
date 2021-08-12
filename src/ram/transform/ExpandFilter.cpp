/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExpandFilter.cpp
 *
 ***********************************************************************/

#include "ram/transform/ExpandFilter.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Utils.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {
class Condition;

namespace ram::transform {

bool ExpandFilterTransformer::expandFilters(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const Filter* filter = as<Filter>(node)) {
            const Condition* condition = &filter->getCondition();
            VecOwn<Condition> conditionList = toConjunctionList(condition);
            if (conditionList.size() > 1) {
                changed = true;
                VecOwn<Filter> filters;
                for (auto& cond : conditionList) {
                    if (filters.empty()) {
                        filters.emplace_back(mk<Filter>(clone(cond), clone(filter->getOperation())));
                    } else {
                        filters.emplace_back(mk<Filter>(clone(cond), std::move(filters.back())));
                    }
                }
                node = std::move(filters.back());
            }
        }

        node->apply(go);
        return node;
    });
    return changed;
}

}  // namespace ram::transform
}  // end of namespace souffle
