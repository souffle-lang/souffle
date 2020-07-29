/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderConditions.cpp
 *
 ***********************************************************************/

#include "ram/transform/ReorderConditions.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Utils.h"
#include "ram/Visitor.h"
#include "ram/analysis/ComplexityAnalysis.h"
#include "utility/MiscUtil.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace souffle {

bool ReorderConditionsTransformer::reorderConditions(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition* condition = &filter->getCondition();
                std::vector<std::unique_ptr<RamCondition>> sortedConds;
                std::vector<std::unique_ptr<RamCondition>> condList = toConjunctionList(condition);
                for (auto& cond : condList) {
                    sortedConds.emplace_back(cond->clone());
                }
                std::sort(sortedConds.begin(), sortedConds.end(),
                        [&](std::unique_ptr<RamCondition>& a, std::unique_ptr<RamCondition>& b) {
                            return rca->getComplexity(a.get()) < rca->getComplexity(b.get());
                        });

                if (!std::equal(sortedConds.begin(), sortedConds.end(), condList.begin(),
                            [](std::unique_ptr<RamCondition>& a, std::unique_ptr<RamCondition>& b) {
                                return *a == *b;
                            })) {
                    changed = true;
                    node = std::make_unique<RamFilter>(
                            std::unique_ptr<RamCondition>(toCondition(sortedConds)),
                            souffle::clone(&filter->getOperation()));
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
