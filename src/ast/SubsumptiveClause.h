/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SubsumptiveClause.h
 *
 * Defines the subsumptive clause class
 *
 ***********************************************************************/

#pragma once

#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/ExecutionPlan.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class Subsumptive Clause
 * @brief Intermediate representation of a subsumptive clause
 *
 * A subsumptive clause has the format
 *
 *      R(...) <= R(...) :- body.
 *
 * Note that both atoms must refer to the same relation R.
 *
 */
class SubsumptiveClause : public Clause {
public:
    SubsumptiveClause(Own<Atom> head, Own<Atom> subsumptiveHead, VecOwn<Literal> bodyLiterals,
            Own<ExecutionPlan> plan = {}, SrcLocation loc = {})
            : Clause(std::move(head), std::move(bodyLiterals), std::move(plan), std::move(loc)),
              subsumptiveHead(std::move(subsumptiveHead)) {
        assert(subsumptiveHead != nullptr && "subsumptive head is a nullptr");
    }

    /** Set subsumptive head */
    void setSubsumptiveHead(Own<Atom> h) {
        subsumptiveHead = std::move(h);
    }

    /** Obtain subsumptive head */
    Atom* getSubsumptiveHead() const {
        return subsumptiveHead.get();
    }

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

    bool equal(const Node& node) const override;

    SubsumptiveClause* cloning() const override;

    /** Subsumptive head */
    Own<Atom> subsumptiveHead;
};

}  // namespace souffle::ast
