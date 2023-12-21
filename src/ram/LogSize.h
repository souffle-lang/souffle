/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LogSize.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/RelationStatement.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class LogSize
 * @brief Log relation size and a logging message.
 */
class LogSize : public RelationStatement {
public:
    LogSize(std::string rel, std::string message)
            : RelationStatement(NK_LogSize, std::move(rel)), message(std::move(message)) {}

    /** @brief Get logging message */
    const std::string& getMessage() const {
        return message;
    }

    LogSize* cloning() const override {
        return new LogSize(relation, message);
    }

    static bool classof(const Node* n) {
        return n->getKind() == NK_LogSize;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "LOG SIZE " << relation;
        os << " TEXT "
           << "\"" << stringify(message) << "\"";
        os << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<LogSize>(node);
        return RelationStatement::equal(other) && message == other.message;
    }

    /** Logging message */
    const std::string message;
};

}  // namespace souffle::ram
