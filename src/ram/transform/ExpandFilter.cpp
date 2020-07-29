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
#include "ram/Utils.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace souffle {
class RamCondition;

bool ExpandFilterTransformer::expandFilters(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition* condition = &filter->getCondition();
                std::vector<std::unique_ptr<RamCondition>> conditionList = toConjunctionList(condition);
                if (conditionList.size() > 1) {
                    changed = true;
                    std::vector<std::unique_ptr<RamFilter>> filters;
                    for (auto& cond : conditionList) {
                        if (filters.empty()) {
                            filters.emplace_back(std::make_unique<RamFilter>(
                                    souffle::clone(cond), souffle::clone(&filter->getOperation())));
                        } else {
                            filters.emplace_back(std::make_unique<RamFilter>(
                                    souffle::clone(cond), std::move(filters.back())));
                        }
                    }
                    node = std::move(filters.back());
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
