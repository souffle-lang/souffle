/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Statement.h
 *
 * Defines abstract class Statement and sub-classes for implementing the
 * Relational Algebra Machine (RAM), which is an abstract machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include <cassert>
#include <ostream>

namespace souffle::ram {

/**
 * @class Statement
 * @brief Abstract class for RAM statements
 */
class Statement : public Node {
public:
    Statement* cloning() const override = 0;

    static bool classof(const Node* n){
        const NodeKind kind = n->getKind();
        return (kind >= NK_Statement && kind < NK_LastStatement);
    }

protected:
    Statement(NodeKind kind) : Node(kind) {
        assert(kind > NK_Statement && kind < NK_LastStatement);
    }

    void print(std::ostream& os) const override {
        print(os, 0);
    }
    /** @brief Pretty print with indentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** @brief Pretty print jump-bed */
    static void print(const Statement* statement, std::ostream& os, int tabpos) {
        assert(statement != nullptr && "statement is a null-pointer");
        statement->print(os, tabpos);
    }

    friend class Program;
};

}  // namespace souffle::ram
