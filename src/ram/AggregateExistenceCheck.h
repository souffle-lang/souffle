/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AggregateExistenceCheck.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Relation.h"
#include "souffle/utility/MiscUtil.h"
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AggregateExistenceCheck
 * @brief Aggregate Existence check for a relation
 */
class AggregateExistenceCheck : public AbstractExistenceCheck {
public:
    AggregateExistenceCheck(std::string rel, VecOwn<Expression> vals)
            : AbstractExistenceCheck(rel, std::move(vals)) {}

    AggregateExistenceCheck* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& cur : values) {
            newValues.emplace_back(cur->cloning());
        }
        return new AggregateExistenceCheck(relation, std::move(newValues));
    }

protected:
    void print(std::ostream& os) const override {
        os << "AGG ";
        AbstractExistenceCheck::print(os);
    }
};

}  // namespace souffle::ram
