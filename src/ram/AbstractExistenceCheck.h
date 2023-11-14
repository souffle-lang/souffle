/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AbstractExistenceCheck.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Relation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AbstractExistenceCheck
 * @brief Abstract existence check for a tuple in a relation
 */
class AbstractExistenceCheck : public Condition {
public:
    AbstractExistenceCheck(std::string rel, VecOwn<Expression> vals)
            : relation(std::move(rel)), values(std::move(vals)) {
        assert(allValidPtrs(values));
    }

    /** @brief Get relation */
    const std::string& getRelation() const {
        return relation;
    }

    /**
     *  @brief Get arguments of the tuple/pattern
     *  A null pointer element in the vector denotes an unspecified
     *  pattern for a tuple element.
     */
    const std::vector<Expression*> getValues() const {
        return toPtrVector(values);
    }

    void apply(const NodeMapper& map) override {
        for (auto& val : values) {
            val = map(std::move(val));
        }
    }

protected:
    void print(std::ostream& os) const override {
        os << "(" << join(values, ",") << ") IN " << relation;
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(EXISTS " << relation << " (" << join(values, " ") << "))";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<AbstractExistenceCheck>(node);
        return relation == other.relation && equal_targets(values, other.values);
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(values);
    }

    /** Relation */
    const std::string relation;

    /** Search tuple */
    VecOwn<Expression> values;
};

}  // namespace souffle::ram
