/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Conjunction.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include "ram/Node.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Conjunction
 * @brief A conjunction of conditions
 *
 * Condition of the form "LHS and RHS", where LHS
 * and RHS are conditions
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * C1 AND C2 AND C3
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Is a Conjunction, which may have LHS "C1"
 * and RHS "C2 AND C3"
 */
class Conjunction : public Condition {
public:
    Conjunction(Own<Condition> l, Own<Condition> r) : lhs(std::move(l)), rhs(std::move(r)) {
        assert(lhs != nullptr && "left-hand side of conjunction is a nullptr");
        assert(rhs != nullptr && "right-hand side of conjunction is a nullptr");
    }

    /** @brief Get left-hand side of conjunction */
    const Condition& getLHS() const {
        return *lhs;
    }

    /** @brief Get right-hand side of conjunction */
    const Condition& getRHS() const {
        return *rhs;
    }

    Conjunction* cloning() const override {
        return new Conjunction(clone(lhs), clone(rhs));
    }

    void apply(const NodeMapper& map) override {
        lhs = map(std::move(lhs));
        rhs = map(std::move(rhs));
    }

protected:
    void print(std::ostream& os) const override {
        os << "(" << *lhs << " AND " << *rhs << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Conjunction>(node);
        return equal_ptr(lhs, other.lhs) && equal_ptr(rhs, other.rhs);
    }

    NodeVec getChildren() const override {
        return {lhs.get(), rhs.get()};
    }

    /** Left-hand side of conjunction */
    Own<Condition> lhs;

    /** Right-hand side of conjunction */
    Own<Condition> rhs;
};

}  // namespace souffle::ram
