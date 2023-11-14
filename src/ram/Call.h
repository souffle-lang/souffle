/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Call.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Statement.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class Call
 * @brief Call a subroutine
 *
 * Calls a subroutine
 *
 * The following example shows how subroutine A is invoked
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * CALL A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

class Call : public Statement {
public:
    Call(std::string name) : name(std::move(name)) {}

    /** @brief Get call name */
    const std::string& getName() const {
        return name;
    }

    Call* cloning() const override {
        return new Call(name);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "CALL " << name << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "(CALL " << name << ")" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Call>(node);
        return name == other.name;
    }

    /** Name of subroutine */
    const std::string name;
};

}  // namespace souffle::ram
