/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IndexIfExists.h
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractIfExists.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/IndexOperation.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class IndexIfExists
 * @brief Use an index to find a tuple in a relation such that a given condition holds.
 *
 * Only one tuple is returned (if one exists), even
 * if multiple tuples satisfying the condition exist.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF ∃ t1 in A ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class IndexIfExists : public IndexOperation, public AbstractIfExists {
public:
    IndexIfExists(std::string rel, std::size_t ident, Own<Condition> cond, RamPattern queryPattern,
            Own<Operation> nested, std::string profileText = "")

            : IndexOperation(rel, ident, std::move(queryPattern), std::move(nested), std::move(profileText)),
              AbstractIfExists(std::move(cond)) {
        assert(getRangePattern().first.size() == getRangePattern().second.size() && "Arity mismatch");
    }

    void apply(const NodeMapper& map) override {
        RelationOperation::apply(map);
        for (auto& pattern : queryPattern.first) {
            pattern = map(std::move(pattern));
        }
        for (auto& pattern : queryPattern.second) {
            pattern = map(std::move(pattern));
        }
        AbstractIfExists::apply(map);
    }

    IndexIfExists* cloning() const override {
        RamPattern resQueryPattern;
        for (const auto& i : queryPattern.first) {
            resQueryPattern.first.emplace_back(i->cloning());
        }
        for (const auto& i : queryPattern.second) {
            resQueryPattern.second.emplace_back(i->cloning());
        }
        auto* res = new IndexIfExists(relation, getTupleId(), clone(condition), std::move(resQueryPattern),
                clone(getOperation()), getProfileText());
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF EXISTS t" << getTupleId() << " IN " << relation;
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        IndexOperation::print(os, tabpos + 1);
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<IndexIfExists>(node);
        return IndexOperation::equal(other) && AbstractIfExists::equal(other);
    }

    NodeVec getChildren() const override {
        auto res = IndexOperation::getChildren();
        res.push_back(AbstractIfExists::getChildren().at(0));
        return res;
    }
};

}  // namespace souffle::ram
