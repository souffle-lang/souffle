/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IndexScan.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/IndexOperation.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/** Pattern type for lower/upper bound */
using RamBound = VecOwn<Expression>;
using RamPattern = std::pair<RamBound, RamBound>;

/**
 * @class IndexScan
 * @brief Search for tuples of a relation matching a criteria
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	 FOR t1 IN X ON INDEX t1.c = t0.0
 *	 ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class IndexScan : public IndexOperation {
public:
    IndexScan(std::string rel, std::size_t ident, RamPattern queryPattern, Own<Operation> nested,
            std::string profileText = "")
            : IndexOperation(rel, ident, std::move(queryPattern), std::move(nested), std::move(profileText)) {
    }

    IndexScan* cloning() const override {
        RamPattern resQueryPattern;
        for (const auto& i : queryPattern.first) {
            resQueryPattern.first.emplace_back(i->cloning());
        }
        for (const auto& i : queryPattern.second) {
            resQueryPattern.second.emplace_back(i->cloning());
        }
        return new IndexScan(
                relation, getTupleId(), std::move(resQueryPattern), clone(getOperation()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "FOR t" << getTupleId() << " IN " << relation;
        printIndex(os);
        os << std::endl;
        IndexOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle::ram
