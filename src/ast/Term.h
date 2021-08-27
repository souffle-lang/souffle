/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Term.h
 *
 * Defines the abstract term class
 *
 ***********************************************************************/

#pragma once

#include "ast/Argument.h"
#include "parser/SrcLocation.h"
#include "souffle/utility/ContainerUtil.h"
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast {

/**
 * @class Term
 * @brief Defines an abstract term class used for functors and other constructors
 */
class Term : public Argument {
protected:
    template <typename... Operands>
    Term(Operands&&... operands) : Term({}, std::forward<Operands>(operands)...) {}

    template <typename... Operands>
    Term(SrcLocation loc, Operands&&... operands)
            : Term(asVec(std::forward<Operands>(operands)...), std::move(loc)) {}

    Term(VecOwn<Argument> operands, SrcLocation loc = {});

public:
    /** Get arguments */
    std::vector<Argument*> getArguments() const;

    /** Add argument to argument list */
    void addArgument(Own<Argument> arg);

    void apply(const NodeMapper& map) override;

    bool equal(const Node& node) const override;

private:
    NodeVec getChildren() const override;

    template <typename... Operands>
    static VecOwn<Argument> asVec(Operands... ops) {
        Own<Argument> ary[] = {std::move(ops)...};
        VecOwn<Argument> xs;
        for (auto&& x : ary) {
            xs.push_back(std::move(x));
        }
        return xs;
    }

protected:
    /** Arguments */
    VecOwn<Argument> args;
};

}  // namespace souffle::ast
