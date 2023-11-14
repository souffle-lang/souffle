/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Insert.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Insert
 * @brief Insert a tuple into the target relation.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     INSERT (t0.a, t0.b, t0.c) INTO @new_X
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Insert : public Operation {
public:
    Insert(std::string rel, VecOwn<Expression> expressions)
            : relation(std::move(rel)), expressions(std::move(expressions)) {
        assert(allValidPtrs(expressions));
    }

    /** @brief Get relation */
    const std::string& getRelation() const {
        return relation;
    }

    /** @brief Get expressions */
    std::vector<Expression*> getValues() const {
        return toPtrVector(expressions);
    }

    Insert* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->cloning());
        }
        return new Insert(relation, std::move(newValues));
    }

    void apply(const NodeMapper& map) override {
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "INSERT (" << join(expressions, ", ", print_deref<Own<Expression>>()) << ") INTO " << relation
           << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(INSERT (";
        os << join(expressions, " ", print_deref<Own<Expression>>());
        os << ") " << relation << ")" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Insert>(node);
        return relation == other.relation && equal_targets(expressions, other.expressions);
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(expressions);
    }

    /** Relation name */
    std::string relation;

    /* Arguments of insert operation */
    VecOwn<Expression> expressions;
};

}  // namespace souffle::ram
