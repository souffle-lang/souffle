/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AbstractLog.h
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
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class AbstractLog
 * @brief Abstract class for logging
 *
 * Comprises a Statement and the message (string) to be logged
 */
class AbstractLog {
public:
    AbstractLog(Own<Statement> stmt, std::string msg) : statement(std::move(stmt)), message(std::move(msg)) {
        assert(statement && "log statement is a nullptr");
    }

    Node::ConstChildNodes getChildNodes() const {
        return Node::ConstChildNodes(getChildren(), detail::RefCaster());
    }

    Node::ChildNodes getChildNodes() {
        return Node::ChildNodes(getChildren(), detail::ConstCaster());
    }

    /** @brief Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    /** @brief Get logging statement */
    const Statement& getStatement() const {
        return *statement;
    }

    void apply(const NodeMapper& map) {
        statement = map(std::move(statement));
    }

protected:
    bool equal(const Node& node) const {
        const auto& other = asAssert<AbstractLog, AllowCrossCast>(node);
        return equal_ptr(statement, other.statement) && message == other.message;
    }

    std::vector<const Node*> getChildren() const {
        return {statement.get()};
    }

    /** Logging statement */
    Own<Statement> statement;

    /** Logging message */
    const std::string message;
};

}  // namespace souffle::ram
