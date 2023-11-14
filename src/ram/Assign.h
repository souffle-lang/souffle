/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Assign.h
 *
 * Defines a class for assigning values to variables in the RAM
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Statement.h"
#include "ram/Variable.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Assign
 * @brief Assigns an expression to a variable
 *
 */
class Assign : public Statement {
public:
    Assign(Own<Variable> v, Own<Expression> e, bool init)
            : variable(std::move(v)), value(std::move(e)), init(init) {
        assert(variable != nullptr && "variable is a null-pointer");
        assert(value != nullptr && "value is a null-pointer");
    }

    /** @brief Get loop body */
    const Variable& getVariable() const {
        return *variable;
    }

    /** @brief Get loop body */
    const Expression& getValue() const {
        return *value;
    }

    bool isInit() const {
        return init;
    }

    Assign* cloning() const override {
        return new Assign(clone(variable), clone(value), init);
    }

    void apply(const NodeMapper& map) override {
        variable = map(std::move(variable));
        value = map(std::move(value));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << (init ? "LET " : "ASSIGN ");
        os << *variable.get() << " := " << *value.get() << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "(ASSIGN ";
        os << *variable.get() << " " << *value.get() << ")" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Assign>(node);
        return equal_ptr(variable, other.variable) && equal_ptr(value, other.value);
    }

    NodeVec getChildren() const override {
        return {variable.get(), value.get()};
    }

    /** Loop body */
    Own<Variable> variable;
    Own<Expression> value;
    bool init;
};

}  // namespace souffle::ram
