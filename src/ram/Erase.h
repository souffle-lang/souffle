/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Erase.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/utility/NodeMapper.h"
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
 * @class Erase
 * @brief Erase a tuple into the target relation.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     ERASE (t0.a, t0.b, t0.c) FROM @delete_X
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Erase : public Operation {
public:
    Erase(std::string rel, VecOwn<Expression> expressions)
            : relation(std::move(rel)), expressions(std::move(expressions)) {
        for (auto const& expr : expressions) {
            assert(expr != nullptr && "Expression is a null-pointer");
        }
    }

    /** @brief Get relation */
    const std::string& getRelation() const {
        return relation;
    }

    /** @brief Get expressions */
    std::vector<Expression*> getValues() const {
        return toPtrVector(expressions);
    }

    NodeVec getChildren() const override {
        NodeVec res;
        for (const auto& expr : expressions) {
            res.push_back(expr.get());
        }
        return res;
    }

    Erase* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->cloning());
        }
        return new Erase(relation, std::move(newValues));
    }

    void apply(const NodeMapper& map) override {
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "ERASE (" << join(expressions, ", ", print_deref<Own<Expression>>()) << ") FROM " << relation
           << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "(ERASE (" << join(expressions, " ", print_deref<Own<Expression>>())
           << ") " << relation << ")" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Erase>(node);
        return relation == other.relation && equal_targets(expressions, other.expressions);
    }

    /** Relation name */
    std::string relation;

    /* Arguments of insert operation */
    VecOwn<Expression> expressions;
};

}  // namespace souffle::ram
