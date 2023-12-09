/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/UserDefinedFunctor.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

UserDefinedFunctor::UserDefinedFunctor(std::string name)
        : Functor(NK_UserDefinedFunctor, {}, {}), name(std::move(name)) {}

UserDefinedFunctor::UserDefinedFunctor(std::string name, VecOwn<Argument> args, SrcLocation loc)
        : Functor(NK_UserDefinedFunctor, std::move(args), std::move(loc)), name(std::move(name)) {}

void UserDefinedFunctor::print(std::ostream& os) const {
    os << '@' << name << "(" << join(args) << ")";
}

bool UserDefinedFunctor::equal(const Node& node) const {
    const auto& other = asAssert<UserDefinedFunctor>(node);
    return name == other.name && Functor::equal(node);
}

UserDefinedFunctor* UserDefinedFunctor::cloning() const {
    return new UserDefinedFunctor(name, clone(args), getSrcLoc());
}

bool UserDefinedFunctor::classof(const Node* n) {
    return n->getKind() == NK_UserDefinedFunctor;
}

}  // namespace souffle::ast
