/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/StringConstant.h"
#include <ostream>
#include <utility>

namespace souffle::ast {

StringConstant::StringConstant(std::string value, SrcLocation loc)
        : Constant(NK_StringConstant, std::move(value), std::move(loc)) {}

void StringConstant::print(std::ostream& os) const {
    os << "\"" << getConstant() << "\"";
}

StringConstant* StringConstant::cloning() const {
    return new StringConstant(getConstant(), getSrcLoc());
}

bool StringConstant::classof(const Node* n) {
    return n->getKind() == NK_StringConstant;
}

}  // namespace souffle::ast
