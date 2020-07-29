/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TupleId.cpp
 *
 ***********************************************************************/

#include "ram/transform/TupleId.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/Visitor.h"
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace souffle {

bool TupleIdTransformer::reorderOperations(RamProgram& program) {
    bool changed = false;
    visitDepthFirst(program, [&](const RamQuery& query) {
        // Maps old tupleIds to new tupleIds
        std::map<int, int> reorder;
        int ctr = 0;

        visitDepthFirst(query, [&](const RamTupleOperation& search) {
            if (ctr != search.getTupleId()) {
                changed = true;
            }
            reorder[search.getTupleId()] = ctr;
            const_cast<RamTupleOperation*>(&search)->setTupleId(ctr);
            ctr++;
        });

        std::function<std::unique_ptr<RamNode>(std::unique_ptr<RamNode>)> elementRewriter =
                [&](std::unique_ptr<RamNode> node) -> std::unique_ptr<RamNode> {
            if (auto* element = dynamic_cast<RamTupleElement*>(node.get())) {
                if (reorder[element->getTupleId()] != element->getTupleId()) {
                    changed = true;
                    node = std::make_unique<RamTupleElement>(
                            reorder[element->getTupleId()], element->getElement());
                }
            }
            node->apply(makeLambdaRamMapper(elementRewriter));
            return node;
        };
        const_cast<RamQuery*>(&query)->apply(makeLambdaRamMapper(elementRewriter));
    });

    return changed;
}

}  // end of namespace souffle
