/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ListStatement.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Statement.h"
#include "ram/utility/NodeMapper.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class ListStatement
 * @brief Abstract class for a list of RAM statements
 */
class ListStatement : public Statement {
public:
    ListStatement() = default;
    ListStatement(VecOwn<Statement> statements) : statements(std::move(statements)) {}

    template <typename... Stmts>
    ListStatement(Own<Stmts>&&... stmts) {
        Own<Statement> tmp[] = {std::move(stmts)...};
        for (auto& cur : tmp) {
            assert(cur.get() != nullptr && "statement is a null-pointer");
            statements.emplace_back(std::move(cur));
        }
    }

    /** @brief Get statements */
    std::vector<Statement*> getStatements() const {
        return toPtrVector(statements);
    }

    void apply(const NodeMapper& map) override {
        for (auto& stmt : statements) {
            stmt = map(std::move(stmt));
        }
    }

protected:
    bool equal(const Node& node) const override {
        const auto& other = asAssert<ListStatement>(node);
        return equal_targets(statements, other.statements);
    }

    NodeVec getChildNodesImpl() const override {
        return toPtrVector<Node const>(statements);
    }

    /** Ordered list of RAM statements */
    VecOwn<Statement> statements;
};

}  // namespace souffle::ram
