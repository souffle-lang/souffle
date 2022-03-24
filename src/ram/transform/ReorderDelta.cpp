/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReorderDelta.cpp
 *
 ***********************************************************************/

#include "ram/transform/ReorderDelta.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Program.h"
#include "ram/Statement.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "ram/utility/NodeMapper.h"
#include "ram/utility/Visitor.h"
#include "souffle/utility/MiscUtil.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace souffle::ram::transform {

bool ReorderDelta::reorderDelta(Program& program) {
    bool changed = false;
    forEachQuery(program, [&](Query& query) {
        std::map<std::size_t, std::size_t> rename;
        query.apply(nodeMapper<Node>([&](auto&& go, Own<Node> node) -> Own<Node> {
            if (const Scan* scan1 = as<Scan>(node)) {
                if (const Scan* scan2 = as<Scan>(scan1->getOperation())) {
                    const auto relation1 = scan1->getRelation();
                    const auto relation2 = scan2->getRelation();
                    const auto id1 = scan1->getTupleId();
                    const auto id2 = scan2->getTupleId();
                    if ("@delta_" + relation1 == relation2) {
                        changed = true;
                        rename[id1] = id2;
                        rename[id2] = id1;
                        auto op = clone(scan2->getOperation());
                        op->apply(nodeMapper<Node>([&](auto&& go, Own<Node> node) -> Own<Node> {
                            if (auto* element = as<TupleElement>(node)) {
                                if (rename[element->getTupleId()] != element->getTupleId()) {
                                    // std::cout << "RENAMING " << *element << std::endl;
                                    changed = true;
                                    node = mk<TupleElement>(
                                            rename[element->getTupleId()], element->getElement());
                                }
                            }
                            node->apply(go);
                            return node;
                        }));
                        auto* inner = new Scan(relation1, id2, std::move(op), scan1->getProfileText());
                        node = Own<Scan>(new Scan(relation2, id1, Own<Scan>(inner), scan2->getProfileText()));
                    }
                }
            }
            node->apply(go);
            return node;
        }));
    });
    return changed;
}

}  // namespace souffle::ram::transform
