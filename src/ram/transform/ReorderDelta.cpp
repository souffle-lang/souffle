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

static bool only_uses_var(std::size_t id, const Expression* e) {
    if (as<UndefValue>(e)) {
        return true;
    }
    if (const auto tup_elem = as<TupleElement>(e)) {
        return tup_elem->getTupleId() == id;
    }
    return false;
}

static Expression* replace_var(std::size_t id, const Expression* e) {
    if (as<UndefValue>(e)) {
        return e->cloning();
    }
    if (const auto tup_elem = as<TupleElement>(e)) {
        return new TupleElement(id, tup_elem->getElement());
    }
    return nullptr;
}

bool ReorderDelta::reorderDelta(Program& program) {
    bool changed = false;
    forEachQueryMap(program, [&](auto&& go, Own<Node> node) -> Own<Node> {
        if (const Scan* scan1 = as<Scan>(node)) {
            if (const IndexScan* scan2 = as<IndexScan>(scan1->getOperation())) {
                const auto relation1 = scan1->getRelation();
                const auto relation2 = scan2->getRelation();
                if ("@delta_" + relation1 == relation2) {
                    const auto pats = scan2->getRangePattern();
                    assert(pats.first.size() == pats.second.size());
                    auto rewrite = true;
                    for (std::size_t i = 0; i < pats.first.size(); i++) {
                        const auto expr1 = pats.first[i];
                        const auto expr2 = pats.second[i];
                        const auto id = scan1->getTupleId();
                        if (!only_uses_var(id, expr1) || !only_uses_var(id, expr2)) {
                            rewrite = false;
                        }
                    }
                    if (rewrite) {
                        changed = true;
                        RamPattern pattern;
                        for (const auto expr : pats.first) {
                            pattern.first.emplace_back(replace_var(scan2->getTupleId(), expr));
                        }
                        for (const auto expr : pats.second) {
                            pattern.second.emplace_back(replace_var(scan2->getTupleId(), expr));
                        }
                        auto* inner = new IndexScan(relation1, scan2->getTupleId(), std::move(pattern),
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
