/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/IntrinsicFunctor.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>

namespace souffle::ast {

IntrinsicFunctor::IntrinsicFunctor(std::string op, VecOwn<Argument> args, SrcLocation loc)
        : Functor(NK_IntrinsicFunctor, std::move(args), std::move(loc)), function(std::move(op)) {}

void IntrinsicFunctor::print(std::ostream& os) const {
    if (isInfixFunctorOp(function)) {
        os << "(" << join(args, function) << ")";
    } else {
        // Negation is handled differently to all other functors so we need a special case.
        if (function == FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME) {
            os << "-";
        } else {
            os << function;
        }
        os << "(" << join(args) << ")";
    }
}

bool IntrinsicFunctor::equal(const Node& node) const {
    const auto& other = asAssert<IntrinsicFunctor>(node);
    return function == other.function && Functor::equal(node);
}

IntrinsicFunctor* IntrinsicFunctor::cloning() const {
    return new IntrinsicFunctor(function, clone(args), getSrcLoc());
}

bool IntrinsicFunctor::classof(const Node* n) {
    return n->getKind() == NK_IntrinsicFunctor;
}

}  // namespace souffle::ast
