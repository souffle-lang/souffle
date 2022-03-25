/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelIndexIfExists.h
 *
 ***********************************************************************/

#pragma once

#include "AggregateOp.h"
#include "ram/AbstractParallel.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/IndexIfExists.h"
#include "ram/IndexOperation.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/utility/Utils.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class ParallelIndexIfExists
 * @brief Use an index to find a tuple in a relation such that a given condition holds in parallel.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    PARALLEL IF ∃t1 in A1  ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class ParallelIndexIfExists : public IndexIfExists, public AbstractParallel {
public:
    ParallelIndexIfExists(std::string rel, std::size_t ident, Own<Condition> cond, RamPattern queryPattern,
            Own<Operation> nested, std::string profileText = "")
            : IndexIfExists(
                      rel, ident, std::move(cond), std::move(queryPattern), std::move(nested), profileText) {}

    ParallelIndexIfExists* cloning() const override {
        RamPattern resQueryPattern;
        for (const auto& i : queryPattern.first) {
            resQueryPattern.first.emplace_back(i->cloning());
        }
        for (const auto& i : queryPattern.second) {
            resQueryPattern.second.emplace_back(i->cloning());
        }
        auto* res = new ParallelIndexIfExists(relation, getTupleId(), clone(condition),
                std::move(resQueryPattern), clone(getOperation()), getProfileText());
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL IF EXISTS t" << getTupleId() << " IN " << relation;
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        IndexOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle::ram
