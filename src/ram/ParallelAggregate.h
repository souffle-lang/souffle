/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParallelAggregate.h
 *
 ***********************************************************************/

#pragma once

#include "parser/AggregateOp.h"
#include "ram/AbstractAggregate.h"
#include "ram/AbstractParallel.h"
#include "ram/Aggregate.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "ram/Utils.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle {

/**
 * @class RamParallelAggregate
 * @brief Parallel Aggregation function applied on some relation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PARALLEL t0.0 = COUNT FOR ALL t0 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Applies the function PARALLEL COUNT to determine the number
 * of elements in A.
 */
class RamParallelAggregate : public RamAggregate, public RamAbstractParallel {
public:
    RamParallelAggregate(std::unique_ptr<RamOperation> nested, AggregateOp fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, int ident)
            : RamAggregate(std::move(nested), fun, std::move(relRef), std::move(expression),
                      std::move(condition), ident) {}

    RamParallelAggregate* clone() const override {
        return new RamParallelAggregate(souffle::clone(&getOperation()), function,
                souffle::clone(relationRef), souffle::clone(expression), souffle::clone(condition),
                identifier);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << "FOR ALL t" << getTupleId() << " ∈ " << getRelation().getName();
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle
