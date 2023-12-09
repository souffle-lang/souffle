/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/NilConstant.h"

namespace souffle::ast {

NilConstant::NilConstant(SrcLocation loc) : Constant(NK_NilConstant, "nil", std::move(loc)) {}

NilConstant* NilConstant::cloning() const {
    return new NilConstant(getSrcLoc());
}

bool NilConstant::classof(const Node* n) {
    return n->getKind() == NK_NilConstant;
}

}  // namespace souffle::ast
