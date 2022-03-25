/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnpackRecord.h
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
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class UnpackRecord
 * @brief Record lookup
 *
 * Looks up a record with respect to an expression
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * UNPACK t0.0 INTO t1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class UnpackRecord : public TupleOperation {
public:
    UnpackRecord(Own<Operation> nested, std::size_t ident, Own<Expression> expr, std::size_t arity)
            : TupleOperation(ident, std::move(nested)), expression(std::move(expr)), arity(arity) {
        assert(expression != nullptr && "Expression is a null-pointer");
    }

    /** @brief Get record expression */
    const Expression& getExpression() const {
        assert(expression != nullptr && "Expression of unpack-record is a null-pointer");
        return *expression;
    }

    /** @brief Get arity of record */
    std::size_t getArity() const {
        return arity;
    }

    UnpackRecord* cloning() const override {
        return new UnpackRecord(clone(getOperation()), getTupleId(), clone(getExpression()), arity);
    }

    void apply(const NodeMapper& map) override {
        TupleOperation::apply(map);
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "UNPACK t" << getTupleId() << " ARITY " << arity << " FROM " << *expression << "\n";
        NestedOperation::print(os, tabpos + 1);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<UnpackRecord>(node);
        return TupleOperation::equal(other) && equal_ptr(expression, other.expression) &&
               arity == other.arity;
    }

    NodeVec getChildren() const override {
        auto res = TupleOperation::getChildren();
        res.push_back(expression.get());
        return res;
    }

    /** Expression for record reference */
    Own<Expression> expression;

    /** Arity of the unpacked tuple */
    const std::size_t arity;
};

}  // namespace souffle::ram
