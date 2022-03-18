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

static std::optional<RamPattern> swap_pattern(
        std::size_t id, const std::pair<std::vector<Expression*>, std::vector<Expression*>>& p) {
    RamPattern pattern;
    std::optional<RamPattern> none;
    const auto size = p.first.size();
    assert(size == p.second.size());
    pattern.first.reserve(size);
    pattern.second.reserve(size);
    for (std::size_t i = 0; i < size; i++) {
        pattern.first.emplace_back(new UndefValue());
    }
    for (std::size_t i = 0; i < size; i++) {
        pattern.second.emplace_back(new UndefValue());
    }
    for (std::size_t i = 0; i < size; i++) {
        const auto* expr = p.first[i];
        if (as<UndefValue>(expr)) {
            continue;
        } else if (const auto tup_elem = as<TupleElement>(expr)) {
            if (tup_elem->getTupleId() != id) {
                return none;
            }
            pattern.second[tup_elem->getElement()] = Own<TupleElement>(new TupleElement(id, i));
        } else {
            return none;
        }
    }
    for (std::size_t i = 0; i < size; i++) {
        const auto* expr = p.second[i];
        if (as<UndefValue>(expr)) {
            continue;
        } else if (const auto tup_elem = as<TupleElement>(expr)) {
            if (tup_elem->getTupleId() != id) {
                return none;
            }
            pattern.first[tup_elem->getElement()] = Own<TupleElement>(new TupleElement(id, i));
        } else {
            return none;
        }
    }
    return std::optional(std::move(pattern));
}

bool ReorderDelta::reorderDelta(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const Scan* scan1 = as<Scan>(node)) {
            if (const IndexScan* scan2 = as<IndexScan>(scan1->getOperation())) {
                const auto relation1 = scan1->getRelation();
                const auto relation2 = scan2->getRelation();
                if ("@delta_" + relation1 == relation2) {
                    std::optional<RamPattern> pattern =
                            swap_pattern(scan1->getTupleId(), scan2->getRangePattern());
                    if (pattern.has_value()) {
                        changed = true;
                        auto* inner = new IndexScan(relation1, scan2->getTupleId(), std::move(*pattern),
                                clone(scan2->getOperation()), scan1->getProfileText());
                        node = Own<Scan>(new Scan(relation2, scan1->getTupleId(), Own<IndexScan>(inner),
                                scan1->getProfileText()));
                    }
                }
            }
        }
        node->apply(go);
        return node;
    });
    return changed;
}

}  // namespace souffle::ram::transform
