/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Argument.h
 *
 * Defines the abstract class for arguments
 *
 ***********************************************************************/

#pragma once

#include "ast/Node.h"

namespace souffle::ast {

/**
 * @class Argument
 * @brief An abstract class for arguments
 */
class Argument : public Node {
public:
    using Node::Node;

    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind >= NK_Argument && kind < NK_LastArgument);
    }
};

}  // namespace souffle::ast
