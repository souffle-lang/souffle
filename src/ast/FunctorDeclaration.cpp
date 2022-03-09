/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/FunctorDeclaration.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/DynamicCasting.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/tinyformat.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace souffle::ast {

FunctorDeclaration::FunctorDeclaration(
        std::string name, VecOwn<Attribute> params, Own<Attribute> returnType, bool stateful, SrcLocation loc)
        : Node(std::move(loc)), name(std::move(name)), params(std::move(params)),
          returnType(std::move(returnType)), stateful(stateful) {
    assert(this->name.length() > 0 && "functor name is empty");
    assert(allValidPtrs(this->params));
    assert(this->returnType != nullptr);
}

void FunctorDeclaration::print(std::ostream& out) const {
    auto convert = [&](Own<Attribute> const& attr) {
        return attr->getName() + ": " + attr->getTypeName().toString();
    };

    tfm::format(out, ".functor %s(%s): %s", name, join(map(params, convert), ","), returnType->getTypeName());
    if (stateful) {
        out << " stateful";
    }
}

bool FunctorDeclaration::equal(const Node& node) const {
    const auto& other = asAssert<FunctorDeclaration>(node);
    return name == other.name && params == other.params && returnType == other.returnType &&
           stateful == other.stateful;
}

FunctorDeclaration* FunctorDeclaration::cloning() const {
    return new FunctorDeclaration(name, clone(params), clone(returnType), stateful, getSrcLoc());
}

}  // namespace souffle::ast
