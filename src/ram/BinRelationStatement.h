/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file BinRelationStatement.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class BinRelationStatement
 * @brief Abstract class for a binary relation
 *
 * Comprises two Relations
 */
class BinRelationStatement : public Statement {
public:
    BinRelationStatement(std::string f, std::string s) : first(std::move(f)), second(std::move(s)) {}

    /** @brief Get first relation */
    const std::string& getFirstRelation() const {
        return first;
    }

    /** @brief Get second relation */
    const std::string& getSecondRelation() const {
        return second;
    }

protected:
    bool equal(const Node& node) const override {
        const auto& other = asAssert<BinRelationStatement>(node);
        return first == other.first && second == other.second;
    }

protected:
    /** First relation */
    const std::string first;

    /** Second relation */
    const std::string second;
};

}  // namespace souffle::ram
