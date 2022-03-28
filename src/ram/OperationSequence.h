/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Sequence.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Operation.h"
#include "souffle/utility/MiscUtil.h"
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class OperationSequence
 * @brief Sequence of RAM operations
 *
 * Execute operations one by one from an ordered list of operations.
 */
class OperationSequence : public Operation {
public:
    OperationSequence() = default;
    OperationSequence(VecOwn<Operation> operations) : operations(std::move(operations)) {}

    template <typename... Opers>
    OperationSequence(Own<Opers>&&... opers) {
        Own<Operation> tmp[] = {std::move(opers)...};
        for (auto& cur : tmp) {
            assert(cur.get() != nullptr && "operation is a null-pointer");
            operations.emplace_back(std::move(cur));
        }
    }

    OperationSequence* cloning() const override {
        auto* res = new OperationSequence();
        for (auto& cur : operations) {
            res->operations.push_back(clone(cur));
        }
        return res;
    }

    /** @brief Get operations */
    std::vector<Operation*> getOperations() const {
        return toPtrVector(operations);
    }

    void apply(const NodeMapper& map) override {
        for (auto& oper : operations) {
            oper = map(std::move(oper));
        }
    }

protected:
    bool equal(const Node& node) const override {
        const auto& other = asAssert<OperationSequence>(node);
        return equal_targets(operations, other.operations);
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(operations);
    }

    void print(std::ostream& os, int tabpos) const override {
        for (const auto& oper : operations) {
            Operation::print(oper.get(), os, tabpos);
        }
    }

    /** Ordered list of RAM statements */
    VecOwn<Operation> operations;
};

}  // namespace souffle::ram
