/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserUtils.h
 *
 * Defines a rewrite class for multi-heads and disjunction
 *
 ***********************************************************************/

#pragma once

#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Literal.h"
#include "souffle/utility/MiscUtil.h"
#include <iosfwd>
#include <utility>
#include <vector>

namespace souffle {

class RuleBody {
public:
    RuleBody negated() const;

    void conjunct(RuleBody other);

    void disjunct(RuleBody other);

    VecOwn<ast::Clause> toClauseBodies() const;

    // -- factory functions --

    static RuleBody getTrue();

    static RuleBody getFalse();

    static RuleBody atom(Own<ast::Atom> atom);

    static RuleBody constraint(Own<ast::Constraint> constraint);

    friend std::ostream& operator<<(std::ostream& out, const RuleBody& body);

private:
    // a struct to represent literals
    struct literal {
        literal(bool negated, Own<ast::Literal> atom) : negated(negated), atom(std::move(atom)) {}

        // whether this literal is negated or not
        bool negated;

        // the atom referenced by tis literal
        Own<ast::Literal> atom;

        literal cloneImpl() const {
            return literal(negated, clone(atom));
        }
    };

    using clause = std::vector<literal>;

    std::vector<clause> dnf;

    static bool equal(const literal& a, const literal& b);

    static bool equal(const clause& a, const clause& b);

    static bool isSubsetOf(const clause& a, const clause& b);

    static void insert(clause& cl, literal&& lit);

    static void insert(std::vector<clause>& cnf, clause&& cls);
};

/**
 * Name unnamed variables in an atom
 */
Own<ast::Atom> nameUnnamedVariables(Own<ast::Atom> atom);

}  // end of namespace souffle
