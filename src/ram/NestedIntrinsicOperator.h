/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file NestedIntrinsicOperator.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/TupleOperation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

enum class NestedIntrinsicOp {
    RANGE,
    URANGE,
    FRANGE,
};

inline std::ostream& operator<<(std::ostream& os, NestedIntrinsicOp e) {
    switch (e) {
        case NestedIntrinsicOp::RANGE: return os << "RANGE";
        case NestedIntrinsicOp::URANGE: return os << "URANGE";
        case NestedIntrinsicOp::FRANGE: return os << "FRANGE";
        default: fatal("invalid Operation");
    }
}

/**
 * @class NestedIntrinsicOperator
 * @brief Effectively identical to `IntrinsicOperator`, except it can produce multiple results.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * RANGE(t0.0, t0.1, t0.2) INTO t1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class NestedIntrinsicOperator : public TupleOperation {
public:
    NestedIntrinsicOperator(
            NestedIntrinsicOp op, VecOwn<Expression> args, Own<Operation> nested, std::size_t ident)
            : TupleOperation(ident, std::move(nested)), args(std::move(args)), op(op) {}

    NestedIntrinsicOp getFunction() const {
        return op;
    }

    std::vector<Expression*> getArguments() const {
        return toPtrVector(args);
    }

    NestedIntrinsicOperator* cloning() const override {
        return new NestedIntrinsicOperator(op, clone(args), clone(getOperation()), getTupleId());
    }

    void apply(const NodeMapper& map) override {
        TupleOperation::apply(map);
        for (auto&& x : args) {
            x = map(std::move(x));
        }
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << op << "(" << join(args, ",", print_deref<Own<Expression>>()) << ") INTO t" << getTupleId()
           << "\n";
        NestedOperation::print(os, tabpos + 1);
    }

    bool equal(const Node& node) const override {
        auto&& other = asAssert<NestedIntrinsicOperator>(node);
        return TupleOperation::equal(node) && op == other.op && equal_targets(args, other.args);
    }

    NodeVec getChildren() const override {
        auto res = TupleOperation::getChildren();
        for (auto&& x : args) {
            res.push_back(x.get());
        }
        return res;
    }

    /* Arguments */
    VecOwn<Expression> args;

    /* Operator */
    const NestedIntrinsicOp op;
};

}  // namespace souffle::ram
