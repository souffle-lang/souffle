/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/RecordInit.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <utility>

namespace souffle::ast {
RecordInit::RecordInit(VecOwn<Argument> operands, SrcLocation loc)
        : Term(NK_RecordInit, std::move(operands), std::move(loc)) {}

void RecordInit::print(std::ostream& os) const {
    os << "[" << join(args) << "]";
}

RecordInit* RecordInit::cloning() const {
    return new RecordInit(clone(args), getSrcLoc());
}

bool RecordInit::classof(const Node* n) {
    return n->getKind() == NK_RecordInit;
}

}  // namespace souffle::ast
