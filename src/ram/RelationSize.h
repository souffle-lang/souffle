/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationSize.h
 *
 * Defines a class for returning the size of a relation.
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Relation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class RelationSize
 * @brief Returns the numbers of tuples in a relation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * size(B)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RelationSize : public Expression {
public:
    RelationSize(std::string rel) : relation(std::move(rel)) {}

    /** @brief Get relation */
    const std::string getRelation() const {
        return relation;
    }

    RelationSize* cloning() const override {
        return new RelationSize(relation);
    }

protected:
    void print(std::ostream& os) const override {
        os << "SIZE(" << relation << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<RelationSize>(node);
        return relation == other.relation;
    }

    /** Relation */
    std::string relation;
};

}  // namespace souffle::ram
