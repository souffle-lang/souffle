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
#include "souffle/utility/Types.h"
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
    Sequence(VecOwn<Statement> statements) : ListStatement(NK_Sequence, std::move(statements)) {}
    Sequence() : ListStatement(NK_Sequence) {}
    template <typename... Stmts>
    Sequence(Own<Statement> first, Own<Stmts>... rest)
            : ListStatement(NK_Sequence, std::move(first), std::move(rest)...) {}

    Sequence* cloning() const override {
        auto* res = new Sequence();
        for (auto& cur : statements) {
            res->statements.push_back(clone(cur));
        }
        return res;
    }

    static bool classof(const Node* n){
        return n->getKind() == NK_Sequence;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        for (const auto& stmt : statements) {
            Statement::print(stmt.get(), os, tabpos);
        }
    }
};

}  // namespace souffle::ram
