/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IO.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/RelationStatement.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class IO
 * @brief I/O statement for a relation
 *
 * I/O operation for a relation, e.g., input/output/printsize
 */
class IO : public RelationStatement {
public:
    IO(std::string rel, std::map<std::string, std::string> directives)
            : RelationStatement(rel), directives(std::move(directives)) {}

    /** @brief get I/O directives */
    const std::map<std::string, std::string>& getDirectives() const {
        return directives;
    }

    /** @get value of I/O directive */
    const std::string get(const std::string& key) const {
        return directives.at(key);
    }

    IO* cloning() const override {
        return new IO(relation, directives);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IO " << relation << " (";
        os << join(directives, ",", [](std::ostream& out, const auto& arg) {
            out << arg.first << "=\"" << escape(arg.second) << "\"";
        });
        os << ")" << std::endl;
    };

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(IO " << relation << " (";
        os << join(directives, " ", [](std::ostream& out, const auto& arg) {
            out << "(= " << arg.first << " \"" << escape(arg.second) << "\"" << ")";
        });
        os << "))" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<IO>(node);
        return RelationStatement::equal(other) && directives == other.directives;
    }

    /** IO directives */
    const std::map<std::string, std::string> directives;
};

}  // namespace souffle::ram
