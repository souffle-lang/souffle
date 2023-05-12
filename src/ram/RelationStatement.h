/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationStatement.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/Statement.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class RelationStatement
 * @brief RAM Statements with a single relation
 */
class RelationStatement : public Statement {
public:
    RelationStatement(std::string rel) : RelationStatement(NK_RelationStatement, std::move(rel)) {}

    /** @brief Get RAM relation */
    const std::string& getRelation() const {
        return relation;
    }

    static bool classof(const Node* n){
        const NodeKind kind = n->getKind();
        return (kind >= NK_RelationStatement && kind < NK_LastRelationStatement);
    }

protected:
    RelationStatement(NodeKind kind, std::string rel) : Statement(kind), relation(std::move(rel)) {
        assert(kind >= NK_RelationStatement && kind < NK_LastRelationStatement);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<RelationStatement>(node);
        return relation == other.relation;
    }

protected:
    /** Relation */
    std::string relation;
};

}  // namespace souffle::ram
