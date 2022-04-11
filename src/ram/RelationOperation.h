/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationOperation.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/TupleOperation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class RelationOperation
 * @brief Abstract class for operations on relations
 *
 * One such operation is a for loop
 */
class RelationOperation : public TupleOperation {
public:
    RelationOperation(std::string rel, std::size_t ident, Own<Operation> nested, std::string profileText = "")
            : TupleOperation(ident, std::move(nested), std::move(profileText)), relation(std::move(rel)) {}

    RelationOperation* cloning() const override = 0;

    /** @brief Get search relation */
    const std::string& getRelation() const {
        return relation;
    }

protected:
    bool equal(const Node& node) const override {
        const auto& other = asAssert<RelationOperation>(node);
        return TupleOperation::equal(other) && relation == other.relation;
    }

    /** Search relation */
    const std::string relation;
};

}  // namespace souffle::ram
