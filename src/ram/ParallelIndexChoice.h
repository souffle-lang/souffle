/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelIndexChoice.h
 *
 ***********************************************************************/

#pragma once

#include "parser/AggregateOp.h"
#include "ram/AbstractParallel.h"
#include "ram/Choice.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/IndexOperation.h"
#include "ram/NestedOperation.h"
#include "ram/Node.h"
#include "ram/NodeMapper.h"
#include "ram/Relation.h"
#include "ram/Utils.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * @class RamParallelIndexChoice
 * @brief Use an index to find a tuple in a relation such that a given condition holds in parallel.
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    PARALLEL CHOICE A AS t1 ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelIndexChoice : public RamIndexChoice, public RamAbstractParallel {
public:
    RamParallelIndexChoice(std::unique_ptr<RamRelationReference> r, int ident,
            std::unique_ptr<RamCondition> cond, RamPattern queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexChoice(std::move(r), ident, std::move(cond), std::move(queryPattern), std::move(nested),
                      profileText) {}

    RamParallelIndexChoice* clone() const override {
        RamPattern resQueryPattern;
        for (const auto& i : queryPattern.first) {
            resQueryPattern.first.emplace_back(i->clone());
        }
        for (const auto& i : queryPattern.second) {
            resQueryPattern.second.emplace_back(i->clone());
        }
        auto* res = new RamParallelIndexChoice(souffle::clone(relationRef), getTupleId(),
                souffle::clone(condition), std::move(resQueryPattern), souffle::clone(&getOperation()),
                getProfileText());
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "PARALLEL CHOICE " << rel.getName() << " AS t" << getTupleId();
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle
