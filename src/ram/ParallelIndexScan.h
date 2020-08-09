/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelIndexScan.h
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractParallel.h"
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
 * @class RamParallelIndexScan
 * @brief Search for tuples of a relation matching a criteria
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	   PARALLEL FOR t1 IN X ON INDEX t1.c = t0.0
 *	     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelIndexScan : public RamIndexScan, public RamAbstractParallel {
public:
    RamParallelIndexScan(std::unique_ptr<RamRelationReference> rel, int ident, RamPattern queryPattern,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamIndexScan(std::move(rel), ident, std::move(queryPattern), std::move(nested), profileText) {}

    RamParallelIndexScan* clone() const override {
        RamPattern resQueryPattern;
        for (const auto& i : queryPattern.first) {
            resQueryPattern.first.emplace_back(i->clone());
        }
        for (const auto& i : queryPattern.second) {
            resQueryPattern.second.emplace_back(i->clone());
        }
        return new RamParallelIndexScan(souffle::clone(relationRef), getTupleId(), std::move(resQueryPattern),
                souffle::clone(&getOperation()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "PARALLEL FOR t" << getTupleId() << " IN ";
        os << rel.getName();
        printIndex(os);
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle
