/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/Type.h"
#include <utility>

namespace souffle::ast {

Type::Type(NodeKind kind, QualifiedName name, SrcLocation loc)
        : Node(kind, std::move(loc)), name(std::move(name)) {
    assert(kind > NK_Type && kind < NK_LastType);
}

void Type::setQualifiedName(QualifiedName name) {
    this->name = std::move(name);
}

bool Type::classof(const Node* n) {
    const NodeKind kind = n->getKind();
    return (kind >= NK_Type && kind < NK_LastType);
}

}  // namespace souffle::ast
