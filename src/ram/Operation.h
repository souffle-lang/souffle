/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Operation.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include <iosfwd>

namespace souffle::ram {

/**
 * @class Operation
 * @brief Abstract class for a relational algebra operation
 */
class Operation : public Node {
public:
    Operation* cloning() const override = 0;

protected:
    void print(std::ostream& os) const override {
        print(os, 0);
    }

    void print_sexpr(std::ostream& os) const override {
        print_sexpr(os, 0);
    }

    /** @brief Pretty print with indentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    virtual void print_sexpr(std::ostream& os, int tabpos) const = 0;

    /** @brief Pretty print jump-bed */
    static void print(const Operation* operation, std::ostream& os, int tabpos) {
        operation->print(os, tabpos);
    }

    static void print_sexpr(const Operation* operation, std::ostream& os, int tabpos) {
        operation->print_sexpr(os, tabpos);
    }

    friend class Query;
};

}  // namespace souffle::ram
