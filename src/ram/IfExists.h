/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IfExists.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractIfExists.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class IfExists
 * @brief Find a tuple in a relation such that a given condition holds.
 *
 * Only one tuple is returned (if one exists), even
 * if multiple tuples satisfying the condition exist.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF ∃ t1 IN A WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class IfExists : public RelationOperation, public AbstractIfExists {
public:
    IfExists(std::string rel, std::size_t ident, Own<Condition> cond, Own<Operation> nested,
            std::string profileText = "")
            : IfExists(NK_IfExists, rel, ident, clone(cond), clone(nested), profileText) {}

    void apply(const NodeMapper& map) override {
        RelationOperation::apply(map);
        AbstractIfExists::apply(map);
    }

    IfExists* cloning() const override {
        return new IfExists(NK_IfExists, relation, getTupleId(), clone(condition), clone(getOperation()),
                getProfileText());
    }

    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind >= NK_IfExists && kind < NK_LastIfExists);
    }

protected:
    IfExists(NodeKind kind, std::string rel, std::size_t ident, Own<Condition> cond, Own<Operation> nested,
            std::string profileText = "")
            : RelationOperation(kind, rel, ident, std::move(nested), std::move(profileText)),
              AbstractIfExists(std::move(cond)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF EXISTS t" << getTupleId();
        os << " IN " << relation;
        os << " WHERE " << getCondition();
        os << std::endl;
        RelationOperation::print(os, tabpos + 1);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<IfExists>(node);
        return RelationOperation::equal(other) && AbstractIfExists::equal(other);
    }

    NodeVec getChildren() const override {
        return {nestedOperation.get(), AbstractIfExists::getChildren().at(0)};
    }
};

}  // namespace souffle::ram
