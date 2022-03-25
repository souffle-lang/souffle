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

#include "AggregateOp.h"
#include "ram/AbstractAggregate.h"
#include "ram/AbstractParallel.h"
#include "ram/Aggregate.h"
#include "ram/Condition.h"
#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "ram/RelationOperation.h"
#include "ram/utility/Utils.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class ParallelAggregate
 * @brief Parallel Aggregation function applied on some relation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PARALLEL t0.0 = COUNT FOR ALL t0 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Applies the function PARALLEL COUNT to determine the number
 * of elements in A.
 */
class ParallelAggregate : public Aggregate, public AbstractParallel {
public:
    ParallelAggregate(Own<Operation> nested, AggregateOp fun, std::string rel, Own<Expression> expression,
            Own<Condition> condition, std::size_t ident)
            : Aggregate(std::move(nested), fun, rel, std::move(expression), std::move(condition), ident) {}

    ParallelAggregate* cloning() const override {
        return new ParallelAggregate(
                clone(getOperation()), function, relation, clone(expression), clone(condition), identifier);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL t" << getTupleId() << ".0 = ";
        AbstractAggregate::print(os, tabpos);
        os << "FOR ALL t" << getTupleId() << " IN " << relation;
        if (!isTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RelationOperation::print(os, tabpos + 1);
    }
};

}  // namespace souffle::ram
