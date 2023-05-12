/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
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
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/QualifiedName.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class SubsumptiveClause
 * @brief Intermediate representation of a subsumptive clause
 *
 * Format:
 *     A(x1,...) <= A(y1,...) :- <Body> .
 *
 */
class SubsumptiveClause : public Clause {
public:
    SubsumptiveClause(
            Own<Atom> head, VecOwn<Literal> bodyLiterals, Own<ExecutionPlan> plan = {}, SrcLocation loc = {});

    SubsumptiveClause(Own<Atom> head, SrcLocation loc = {});

    SubsumptiveClause(QualifiedName name, SrcLocation loc = {});

    /** Add a literal to the body of the clause as a first literal */
    void addToBodyFront(Own<Literal> literal);

    Clause* cloneHead() const override;

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    SubsumptiveClause* cloning() const override;
};

}  // namespace souffle::ast
