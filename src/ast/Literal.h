/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Literal.h
 *
 * Defines the literal class
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"

namespace souffle::ast {

/**
 * @class Literal
 * @brief Defines an abstract class for literals in a horn clause.
 *
 * Literals can be atoms, binary relations, and negated atoms
 * in the body and head of a clause.
 */
class Literal : public Node {
public:
    using Node::Node;

    explicit Literal(NodeKind kind, SrcLocation loc = {}) : Node(kind, std::move(loc)) {
        assert(kind >= NK_Literal && kind < NK_LastLiteral);
    }

    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind >= NK_Literal && kind < NK_LastLiteral);
    }
};

}  // namespace souffle::ast
