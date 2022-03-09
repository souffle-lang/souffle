/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Clause.h
 *
 * Defines the clause class
 *
 ***********************************************************************/

#pragma once

#include "ast/Atom.h"
#include "ast/ExecutionPlan.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class Clause
 * @brief Intermediate representation of a horn clause
 *
 *  A clause can either be:
 *      - a fact  - a clause with no body (e.g., X(a,b))
 *      - a rule  - a clause with a head and a body (e.g., Y(a,b) -: X(a,b))
 */
class Clause : public Node {
public:
    Clause(Own<Atom> head, VecOwn<Literal> bodyLiterals, Own<ExecutionPlan> plan = {}, SrcLocation loc = {});

    Clause(Own<Atom> head, SrcLocation loc = {});

    Clause(QualifiedName name, SrcLocation loc = {});

    /** Add a literal to the body of the clause */
    void addToBody(Own<Literal> literal);

    /** Add a collection of literals to the body of the clause */
    void addToBody(VecOwn<Literal>&& literals);

    /** Set the head of clause to @p h */
    void setHead(Own<Atom> h);

    /** Set the body literals of clause to @p body */
    void setBodyLiterals(VecOwn<Literal> body);

    /** Return the atom that represents the head of the clause */
    Atom* getHead() const {
        return head.get();
    }

    /** Obtains a copy of the internally maintained body literals */
    std::vector<Literal*> getBodyLiterals() const;

    /** Obtains the execution plan associated to this clause or null if there is none */
    const ExecutionPlan* getExecutionPlan() const {
        return plan.get();
    }

    /** Updates the execution plan associated to this clause */
    void setExecutionPlan(Own<ExecutionPlan> plan);

    /** Resets the execution plan */
    void clearExecutionPlan() {
        plan = nullptr;
    }

    /** Clones the head/source location/plan of a clause without the body */
    virtual Clause* cloneHead() const;

    void apply(const NodeMapper& map) override;

protected:
    void print(std::ostream& os) const override;

    NodeVec getChildren() const override;

    bool equal(const Node& node) const override;

    Clause* cloning() const override;

protected:
    /** Head of the clause */
    Own<Atom> head;

    /** Body literals of clause */
    VecOwn<Literal> bodyLiterals;

    /** User defined execution plan (if not defined, plan is null) */
    Own<ExecutionPlan> plan;
};

}  // namespace souffle::ast
