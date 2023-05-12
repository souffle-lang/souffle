/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Condition.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"

namespace souffle::ram {

/**
 * @class Condition
 * @brief Abstract class for conditions and boolean values in RAM
 */
class Condition : public Node {
protected:
    using Node::Node;

    Condition(NodeKind kind) : Node(kind) {
        assert(kind >= NK_Condition && kind < NK_LastCondition);
    }

public:
    Condition* cloning() const override = 0;

    static bool classof(const Node* n){
        const NodeKind kind = n->getKind();
        return (kind >= NK_Condition && kind < NK_LastCondition);
    }
};

}  // namespace souffle::ram
