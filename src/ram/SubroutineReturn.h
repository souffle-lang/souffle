/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SubroutineReturn.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class SubroutineReturn
 * @brief A statement for returning from a ram subroutine
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   ...
 *     RETURN (t0.0, t0.1)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class SubroutineReturn : public Operation {
public:
    SubroutineReturn(VecOwn<Expression> vals) : expressions(std::move(vals)) {
        assert(allValidPtrs(expressions));
    }

    /** @brief Getter for expressions */
    std::vector<Expression*> getValues() const {
        return toPtrVector(expressions);
    }

    SubroutineReturn* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->cloning());
        }
        return new SubroutineReturn(std::move(newValues));
    }

    void apply(const NodeMapper& map) override {
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "RETURN (";
        for (auto val : getValues()) {
            os << *val;
            if (val != *(getValues().end() - 1)) {
                os << ", ";
            }
        }
        os << ")" << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(RETURN ";
        os << join(expressions, " ") << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<SubroutineReturn>(node);
        return equal_targets(expressions, other.expressions);
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(expressions);
    }

    /** Return expressions */
    VecOwn<Expression> expressions;
};

}  // namespace souffle::ram
