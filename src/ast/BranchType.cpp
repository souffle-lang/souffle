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

BranchType::BranchType(QualifiedName name, VecOwn<Attribute> fields, SrcLocation loc)
        : Node(std::move(loc)), name(std::move(name)), fields(std::move(fields)) {
    assert(allValidPtrs(this->fields));
}

std::vector<Attribute*> BranchType::getFields() {
    return toPtrVector(fields);
}

void BranchType::print(std::ostream& os) const {
    os << tfm::format("%s {%s}", name, join(fields, ", "));
}

BranchType* BranchType::cloning() const {
    return new BranchType(name, clone(fields), getSrcLoc());
}

void BranchType::setFieldType(std::size_t idx, QualifiedName type) {
    fields.at(idx)->setTypeName(std::move(type));
}

}  // namespace souffle::ast
