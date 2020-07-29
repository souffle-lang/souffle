/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderFilterBreak.cpp
 *
 ***********************************************************************/

#include "ram/transform/ReorderFilterBreak.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <functional>
#include <memory>
#include <vector>

namespace souffle {

bool ReorderFilterBreak::reorderFilterBreak(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            // find filter-break nesting
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                if (const RamBreak* br = dynamic_cast<RamBreak*>(&filter->getOperation())) {
                    changed = true;
                    // convert to break-filter nesting
                    node = std::make_unique<RamBreak>(souffle::clone(&br->getCondition()),
                            std::make_unique<RamFilter>(souffle::clone(&filter->getCondition()),
                                    souffle::clone(&br->getOperation())));
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
