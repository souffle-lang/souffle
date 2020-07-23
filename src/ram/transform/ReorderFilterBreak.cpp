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
#include "ram/RamCondition.h"
#include "ram/RamNode.h"
#include "ram/RamOperation.h"
#include "ram/RamProgram.h"
#include "ram/RamStatement.h"
#include "ram/RamVisitor.h"
#include "souffle/utility/MiscUtil.h"
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
