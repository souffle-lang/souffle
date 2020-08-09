/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
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

#include "parser/AggregateOp.h"
#include "ram/AbstractAggregate.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/IndexOperation.h"
#include "ram/Node.h"
#include "ram/NodeMapper.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
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
 * @class RamIndexAggregate
 * @brief Indexed aggregation on a relation. The index allows us to iterate over a restricted range
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0=sum t0.1 SEARCH t0 ∈ S ON INDEX t0.0 = number(1)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamIndexAggregate : public RamIndexOperation, public RamAbstractAggregate {
public:
    RamIndexAggregate(std::unique_ptr<RamOperation> nested, AggregateOp fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, RamPattern queryPattern, int ident)
            : RamIndexOperation(std::move(relRef), ident, std::move(queryPattern), std::move(nested)),
              RamAbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamIndexOperation::getChildNodes();
        auto children = RamAbstractAggregate::getChildNodes();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }

    RamIndexAggregate* clone() const override {
        RamPattern pattern;
        for (const auto& i : queryPattern.first) {
            pattern.first.emplace_back(i->clone());
        }
        for (const auto& i : queryPattern.second) {
            pattern.second.emplace_back(i->clone());
        }
        return new RamIndexAggregate(souffle::clone(&getOperation()), function, souffle::clone(relationRef),
                souffle::clone(expression), souffle::clone(condition), std::move(pattern), getTupleId());
    }

    void apply(const RamNodeMapper& map) override {
        RamIndexOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << "SEARCH t" << getTupleId() << " ∈ " << getRelation().getName();
        printIndex(os);
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamIndexAggregate&>(node);
        return RamIndexOperation::equal(other) && RamAbstractAggregate::equal(other);
    }
};

}  // namespace souffle
