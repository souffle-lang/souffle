/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Counter.h"
#include <ostream>

namespace souffle::ast {

Counter::Counter(SrcLocation loc) : Argument(NK_Counter, std::move(loc)) {}

void Counter::print(std::ostream& os) const {
    os << "$";
}

Counter* Counter::cloning() const {
    return new Counter(getSrcLoc());
}

bool Counter::classof(const Node* n) {
    return n->getKind() == NK_Counter;
}

bool Counter::equal(const Node&) const {
    return true;
}

}  // namespace souffle::ast
