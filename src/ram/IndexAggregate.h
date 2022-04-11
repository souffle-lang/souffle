/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IndexAggregate.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "ram/AbstractAggregate.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/IndexOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/utility/Utils.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class IndexAggregate
 * @brief Indexed aggregation on a relation. The index allows us to iterate over a restricted range
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0=sum t0.1 SEARCH t0 ∈ S ON INDEX t0.0 = number(1)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class IndexAggregate : public IndexOperation, public AbstractAggregate {
public:
    IndexAggregate(Own<Operation> nested, AggregateOp fun, std::string rel, Own<Expression> expression,
            Own<Condition> condition, RamPattern queryPattern, std::size_t ident)
            : IndexOperation(rel, ident, std::move(queryPattern), std::move(nested)),
              AbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    IndexAggregate* cloning() const override {
        RamPattern pattern;
        for (const auto& i : queryPattern.first) {
            pattern.first.emplace_back(i->cloning());
        }
        for (const auto& i : queryPattern.second) {
            pattern.second.emplace_back(i->cloning());
        }
        return new IndexAggregate(clone(getOperation()), function, relation, clone(expression),
                clone(condition), std::move(pattern), getTupleId());
    }

    void apply(const NodeMapper& map) override {
        IndexOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0 = ";
        AbstractAggregate::print(os, tabpos);
        os << "SEARCH t" << getTupleId() << " IN " << relation;
        printIndex(os);
        if (!isTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        IndexOperation::print(os, tabpos + 1);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<IndexAggregate>(node);
        return IndexOperation::equal(other) && AbstractAggregate::equal(other);
    }

    NodeVec getChildren() const override {
        auto res = IndexOperation::getChildren();
        auto children = AbstractAggregate::getChildren();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }
};

}  // namespace souffle::ram
