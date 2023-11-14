/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Sequence.h
 *
 ***********************************************************************/

#pragma once

#include "ram/ListStatement.h"
#include "ram/Statement.h"
#include "souffle/utility/MiscUtil.h"
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class Sequence
 * @brief Sequence of RAM statements
 *
 * Execute statement one by one from an ordered list of statements.
 */
class Sequence : public ListStatement {
public:
    Sequence(VecOwn<Statement> statements) : ListStatement(std::move(statements)) {}
    Sequence() : ListStatement() {}
    template <typename... Stmts>
    Sequence(Own<Statement> first, Own<Stmts>... rest)
            : ListStatement(std::move(first), std::move(rest)...) {}

    Sequence* cloning() const override {
        auto* res = new Sequence();
        for (auto& cur : statements) {
            res->statements.push_back(clone(cur));
        }
        return res;
    }

    int get_non_empty_size() const {
        int non_empty_size = 0;
        for (const auto& stmt : statements) {
            if (!isA<Sequence>(stmt.get())) {
                non_empty_size++;
            } else {
                non_empty_size += as<Sequence>(stmt.get())->get_non_empty_size();
            }
        }
        return non_empty_size;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        for (const auto& stmt : statements) {
            Statement::print(stmt.get(), os, tabpos);
        }
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        int non_empty_size = get_non_empty_size();
        if (non_empty_size == 0) {
            return;
        }
        if (non_empty_size == 1) {
            if (isA<Sequence>(statements[0].get())) {
                Statement::print_sexpr(statements[0].get(), os, tabpos);
                return;
            }
        }
        os << "(STMTS ";
        for (const auto& stmt : statements) {
            Statement::print_sexpr(stmt.get(), os, tabpos);
            os << " ";
        }
        os << ")";
    }
};

}  // namespace souffle::ram
