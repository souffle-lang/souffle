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
    Term(NodeKind kind, Operands&&... operands) : Term(kind, {}, std::forward<Operands>(operands)...) {}

    template <typename... Operands>
    Term(NodeKind kind, SrcLocation loc, Operands&&... operands)
            : Term(kind, asVec(std::forward<Operands>(operands)...), std::move(loc)) {}

    Term(NodeKind kind, VecOwn<Argument> operands, SrcLocation loc = {});

public:
    /** Get arguments */
    std::vector<Argument*> getArguments() const;

    /** Add argument to argument list */
    void addArgument(Own<Argument> arg);

    void apply(const NodeMapper& map) override;

    bool equal(const Node& node) const override;

    static bool classof(const Node*);

private:
    NodeVec getChildren() const override;

    template <typename... Operands>
    static VecOwn<Argument> asVec(Operands... ops) {
        VecOwn<Argument> xs;
        (xs.emplace_back(std::move(std::forward<Operands>(ops))), ...);
        return xs;
    }

protected:
    /** Arguments */
    VecOwn<Argument> args;
};

}  // namespace souffle::ast
