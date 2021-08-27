/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Term.h"

#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>

namespace souffle::ast {

Term::Term(VecOwn<Argument> operands, SrcLocation loc) : Argument(std::move(loc)), args(std::move(operands)) {
    assert(allValidPtrs(args));
}

std::vector<Argument*> Term::getArguments() const {
    return toPtrVector(args);
}

/** Add argument to argument list */
void Term::addArgument(Own<Argument> arg) {
    assert(arg != nullptr);
    args.push_back(std::move(arg));
}

Node::NodeVec Term::getChildren() const {
    auto res = Argument::getChildren();
    append(res, makePtrRange(args));
    return res;
}

void Term::apply(const NodeMapper& map) {
    mapAll(args, map);
}

bool Term::equal(const Node& node) const {
    const auto& other = asAssert<Term>(node);
    return equal_targets(args, other.args);
}

}  // namespace souffle::ast
