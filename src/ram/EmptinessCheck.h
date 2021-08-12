/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file EmptinessCheck.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
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
 * @class EmptinessCheck
 * @brief Emptiness check for a relation
 *
 * Evaluates to true if the given relation is the empty set
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (B = ∅)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class EmptinessCheck : public Condition {
public:
    EmptinessCheck(std::string rel) : relation(std::move(rel)) {}

    /** @brief Get relation */
    const std::string& getRelation() const {
        return relation;
    }

    EmptinessCheck* cloning() const override {
        return new EmptinessCheck(relation);
    }

protected:
    void print(std::ostream& os) const override {
        os << "ISEMPTY(" << relation << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<EmptinessCheck>(node);
        return relation == other.relation;
    }

    /** Relation */
    const std::string relation;
};

}  // namespace souffle::ram
