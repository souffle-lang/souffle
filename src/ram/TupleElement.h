/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TupleElement.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "souffle/utility/MiscUtil.h"
#include <cstdlib>
#include <ostream>

namespace souffle::ram {

/**
 * @class TupleElement
 * @brief Access element from the current tuple in a tuple environment
 *
 * In the following example, the tuple element t0.1 is accessed:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * IF t0.1 in A
 * 	...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class TupleElement : public Expression {
public:
    TupleElement(std::size_t ident, std::size_t elem) : identifier(ident), element(elem) {}

    /** @brief Get identifier */
    std::size_t getTupleId() const {
        return identifier;
    }

    /** @brief Get element */
    std::size_t getElement() const {
        return element;
    }

    TupleElement* cloning() const override {
        return new TupleElement(identifier, element);
    }

protected:
    void print(std::ostream& os) const override {
        os << "t" << identifier << "." << element;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<TupleElement>(node);
        return identifier == other.identifier && element == other.element;
    }

    /** Identifier for the tuple */
    const std::size_t identifier;

    /** Element number */
    const std::size_t element;
};

}  // namespace souffle::ram
