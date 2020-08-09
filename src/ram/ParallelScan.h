/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelScan.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractParallel.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/NodeMapper.h"
#include "ram/Relation.h"
#include "ram/Scan.h"
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
 * @class RamParallelScan
 * @brief Iterate all tuples of a relation in parallel
 *
 * An example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *   PARALLEL FOR t0 IN A
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RamParallelScan : public RamScan, public RamAbstractParallel {
public:
    RamParallelScan(std::unique_ptr<RamRelationReference> rel, int ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamScan(std::move(rel), ident, std::move(nested), profileText) {}

    RamParallelScan* clone() const override {
        return new RamParallelScan(
                souffle::clone(relationRef), getTupleId(), souffle::clone(&getOperation()), getProfileText());
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL FOR t" << getTupleId();
        os << " IN " << getRelation().getName() << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }
};
}  // namespace souffle
