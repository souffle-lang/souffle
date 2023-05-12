/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IntrinsicFunctor.h
 *
 * Defines the intrinsic functor class
 *
 ***********************************************************************/

#pragma once

#include "FunctorOps.h"
#include "ast/Argument.h"
#include "ast/Functor.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <string>
#include <utility>

namespace souffle::ast {

/**
 * @class IntrinsicFunctor
 * @brief Intrinsic Functor class for functors are in-built.
 */
class IntrinsicFunctor : public Functor {
public:
    template <typename... Operands>
    IntrinsicFunctor(std::string op, Operands&&... operands)
            : Functor(NK_IntrinsicFunctor, std::forward<Operands>(operands)...), function(std::move(op)) {}

    template <typename... Operands>
    IntrinsicFunctor(SrcLocation loc, std::string op, Operands&&... operands)
            : Functor(NK_IntrinsicFunctor, std::move(loc), std::forward<Operands>(operands)...), function(std::move(op)) {}

    IntrinsicFunctor(std::string op, VecOwn<Argument> args, SrcLocation loc = {});

    /** Get (base type) function */
    const std::string& getBaseFunctionOp() const {
        return function;
    }

    /** Set function */
    void setFunction(std::string functor) {
        function = std::move(functor);
    }

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    IntrinsicFunctor* cloning() const override;

private:
    /** Function */
    std::string function;
};

}  // namespace souffle::ast
