/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/IterationCounter.h"
#include <ostream>

namespace souffle::ast {

IterationCounter::IterationCounter(SrcLocation loc) : Argument(NK_IterationCounter, loc) {}

void IterationCounter::print(std::ostream& os) const {
    os << "recursive_iteration_cnt()";
}

IterationCounter* IterationCounter::cloning() const {
    return new IterationCounter(getSrcLoc());
}

bool IterationCounter::classof(const Node* n) {
    return n->getKind() == NK_IterationCounter;
}

}  // namespace souffle::ast
