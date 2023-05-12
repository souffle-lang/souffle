/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Expression.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Node.h"
#include <cassert>

namespace souffle::ram {

/**
 * @class Expression
 * @brief Abstract class for describing scalar values in RAM
 */
class Expression : public Node {
protected:
    using Node::Node;

    explicit Expression(NodeKind kind) : Node(kind) {
        assert(kind >= NK_Expression && kind < NK_LastExpression);
    }

public:
    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind >= NK_Expression && kind < NK_LastExpression);
    }

    Expression* cloning() const override = 0;
};

}  // namespace souffle::ram
