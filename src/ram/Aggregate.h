/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
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

#include "parser/AggregateOp.h"
#include "ram/AbstractAggregate.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/NodeMapper.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "ram/Utils.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamAggregate
 * @brief Aggregation function applied on some relation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0 = COUNT FOR ALL t0 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Applies the function COUNT to determine the number
 * of elements in A.
 */
class RamAggregate : public RamRelationOperation, public RamAbstractAggregate {
public:
    RamAggregate(std::unique_ptr<RamOperation> nested, AggregateOp fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, int ident)
            : RamRelationOperation(std::move(relRef), ident, std::move(nested)),
              RamAbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamRelationOperation::getChildNodes();
        auto children = RamAbstractAggregate::getChildNodes();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }

    RamAggregate* clone() const override {
        return new RamAggregate(souffle::clone(&getOperation()), function, souffle::clone(relationRef),
                souffle::clone(expression), souffle::clone(condition), getTupleId());
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << "FOR ALL t" << getTupleId() << " ∈ " << getRelation().getName();
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamAggregate&>(node);
        return RamRelationOperation::equal(other) && RamAbstractAggregate::equal(node);
    }
};

}  // namespace souffle
