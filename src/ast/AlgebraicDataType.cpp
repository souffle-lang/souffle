/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/AlgebraicDataType.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/tinyformat.h"
#include <cassert>

namespace souffle::ast {

AlgebraicDataType::AlgebraicDataType(QualifiedName name, VecOwn<BranchType> branches, SrcLocation loc)
        : Type(std::move(name), std::move(loc)), branches(std::move(branches)) {
    assert(!this->branches.empty());
    assert(allValidPtrs(this->branches));
}

std::vector<BranchType*> AlgebraicDataType::getBranches() const {
    return toPtrVector(branches);
}

Node::NodeVec AlgebraicDataType::getChildren() const {
    auto cn = makePtrRange(branches);
    return {cn.begin(), cn.end()};
}

void AlgebraicDataType::print(std::ostream& os) const {
    os << tfm::format(".type %s = %s", getQualifiedName(), join(branches, " | "));
}

bool AlgebraicDataType::equal(const Node& node) const {
    const auto& other = asAssert<AlgebraicDataType>(node);
    return getQualifiedName() == other.getQualifiedName() && branches == other.branches;
}

AlgebraicDataType* AlgebraicDataType::cloning() const {
    return new AlgebraicDataType(getQualifiedName(), clone(branches), getSrcLoc());
}

}  // namespace souffle::ast
