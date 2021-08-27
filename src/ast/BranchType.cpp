/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/BranchType.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/tinyformat.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

BranchType::BranchType(std::string constructor, VecOwn<Attribute> fields, SrcLocation loc)
        : Node(std::move(loc)), constructor(std::move(constructor)), fields(std::move(fields)) {
    assert(allValidPtrs(this->fields));
}

std::vector<Attribute*> BranchType::getFields() {
    return toPtrVector(fields);
}

void BranchType::print(std::ostream& os) const {
    os << tfm::format("%s {%s}", constructor, join(fields, ", "));
}

BranchType* BranchType::cloning() const {
    return new BranchType(constructor, clone(fields), getSrcLoc());
}

}  // namespace souffle::ast
