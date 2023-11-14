/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Parallel.h
 *
 ***********************************************************************/

#pragma once

#include "ram/ListStatement.h"
#include "ram/Statement.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Parallel
 * @brief Parallel block of statements
 *
 * Execute statements in parallel and wait until all statements have
 * completed their execution before completing the execution of the
 * parallel block.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PARALLEL
 *   BEGIN DEBUG...
 *     QUERY
 *       ...
 * END PARALLEL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Parallel : public ListStatement {
public:
    Parallel(VecOwn<Statement> statements) : ListStatement(std::move(statements)) {}
    Parallel() : ListStatement() {}
    template <typename... Stmts>
    Parallel(Own<Statement> first, Own<Stmts>... rest)
            : ListStatement(std::move(first), std::move(rest)...) {}

    Parallel* cloning() const override {
        auto* res = new Parallel();
        for (auto& cur : statements) {
            res->statements.push_back(clone(cur));
        }
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "PARALLEL" << std::endl;
        for (auto const& stmt : statements) {
            Statement::print(stmt.get(), os, tabpos + 1);
        }
        os << times(" ", tabpos) << "END PARALLEL" << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "(PARALLEL " << std::endl;
        for (auto const& stmt : statements) {
            Statement::print_sexpr(stmt.get(), os, tabpos + 1);
        }
        os << times(" ", tabpos) << ")" << std::endl;
    }
};

}  // namespace souffle::ram
