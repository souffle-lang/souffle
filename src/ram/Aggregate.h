/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Aggregate.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "ram/AbstractAggregate.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
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
 * @class Aggregate
 * @brief Aggregation function applied on some relation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0 = COUNT FOR ALL t0 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Applies the function COUNT to determine the number
 * of elements in A.
 */
class Aggregate : public RelationOperation, public AbstractAggregate {
public:
    Aggregate(Own<Operation> nested, AggregateOp fun, std::string rel, Own<Expression> expression,
            Own<Condition> condition, std::size_t ident)
            : RelationOperation(rel, ident, std::move(nested)),
              AbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    Aggregate* cloning() const override {
        return new Aggregate(
                clone(getOperation()), function, relation, clone(expression), clone(condition), getTupleId());
    }

    void apply(const NodeMapper& map) override {
        RelationOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0 = ";
        AbstractAggregate::print(os, tabpos);
        os << "FOR ALL t" << getTupleId() << " IN " << getRelation();
        if (!isTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RelationOperation::print(os, tabpos + 1);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Aggregate>(node);
        return RelationOperation::equal(other) && AbstractAggregate::equal(node);
    }

    NodeVec getChildren() const override {
        auto res = RelationOperation::getChildren();
        auto children = AbstractAggregate::getChildren();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }
};

}  // namespace souffle::ram
