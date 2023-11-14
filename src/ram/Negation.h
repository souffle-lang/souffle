/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Negation.h
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
 * @class Negation
 * @brief Negates a given condition
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (NOT t0 IN A)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Negation : public Condition {
public:
    Negation(Own<Condition> op) : operand(std::move(op)) {
        assert(operand != nullptr && "operand of negation is a null-pointer");
    }

    /** @brief Get operand of negation */
    const Condition& getOperand() const {
        return *operand;
    }

    Negation* cloning() const override {
        return new Negation(clone(operand));
    }

    void apply(const NodeMapper& map) override {
        operand = map(std::move(operand));
    }

protected:
    void print(std::ostream& os) const override {
        os << "(NOT " << *operand << ")";
    }

    void print_sexpr(std::ostream& os) const override {
        os << "(NOT " << *operand << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Negation>(node);
        return equal_ptr(operand, other.operand);
    }

    NodeVec getChildren() const override {
        return {operand.get()};
    }

    /** Operand */
    Own<Condition> operand;
};

}  // namespace souffle::ram
