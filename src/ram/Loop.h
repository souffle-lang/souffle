/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Loop.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Statement.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Loop
 * @brief Execute statement until statement terminates loop via an exit statement
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * LOOP
 *   PARALLEL
 *     ...
 *   END PARALLEL
 * END LOOP
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Loop : public Statement {
public:
    Loop(Own<Statement> b) : body(std::move(b)) {
        assert(body != nullptr && "Loop body is a null-pointer");
    }

    /** @brief Get loop body */
    const Statement& getBody() const {
        return *body;
    }

    Loop* cloning() const override {
        return new Loop(clone(body));
    }

    void apply(const NodeMapper& map) override {
        body = map(std::move(body));
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "LOOP" << std::endl;
        Statement::print(body.get(), os, tabpos + 1);
        os << times(" ", tabpos) << "END LOOP" << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << "(LOOP\n";
        Statement::print_sexpr(body.get(), os, tabpos + 1);
        os << times(" ", tabpos) << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<Loop>(node);
        return equal_ptr(body, other.body);
    }

    NodeVec getChildren() const override {
        return {body.get()};
    }

    /** Loop body */
    Own<Statement> body;
};

}  // namespace souffle::ram
