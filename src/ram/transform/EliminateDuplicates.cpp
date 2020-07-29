/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file EliminateDuplicates.cpp
 *
 ***********************************************************************/

#include "ram/transform/EliminateDuplicates.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Utils.h"
#include "ram/Visitor.h"
#include "utility/MiscUtil.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace souffle {

bool EliminateDuplicatesTransformer::eliminateDuplicates(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> filterRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (const RamFilter* filter = dynamic_cast<RamFilter*>(node.get())) {
                const RamCondition* condition = &filter->getCondition();
                std::vector<std::unique_ptr<RamCondition>> conds = toConjunctionList(condition);
                bool eliminatedDuplicate = false;
                for (std::size_t i = 0; i < conds.size(); i++) {
                    for (std::size_t j = i + 1; j < conds.size(); j++) {
                        if (*conds[i] == *conds[j]) {
                            conds.erase(conds.begin() + j);
                            i = -1;
                            eliminatedDuplicate = true;
                            break;
                        }
                    }
                }
                if (eliminatedDuplicate) {
                    changed = true;
                    node = std::make_unique<RamFilter>(std::unique_ptr<RamCondition>(toCondition(conds)),
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
