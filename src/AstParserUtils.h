/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstParserUtils.h
 *
 * Defines class RuleBody to represents rule bodies.
 *
 ***********************************************************************/

#pragma once

#include "AstLiteral.h"
#include "Util.h"

#include <memory>
#include <vector>

namespace souffle {

class RuleBody {
    // a struct to represent literals
    struct literal {
        // whether this literal is negated or not
        bool negated;

        // the atom referenced by tis literal
        std::unique_ptr<AstLiteral> atom;

        literal clone() const {
            return literal({negated, std::unique_ptr<AstLiteral>(atom->clone())});
        }
    };

    using clause = std::vector<literal>;
    std::vector<clause> dnf;

public:
    RuleBody() {}

    void negate();

    void conjunct(RuleBody&& other);

    void disjunct(RuleBody&& other);

    std::vector<AstClause*> toClauseBodies() const;

    // -- factory functions --

    static RuleBody getTrue();

    static RuleBody getFalse();

    static RuleBody atom(AstAtom* atom);

    static RuleBody constraint(AstConstraint* constraint);

    friend std::ostream& operator<<(std::ostream& out, const RuleBody& body);

private:
    static bool equal(const literal& a, const literal& b);

    static bool equal(const clause& a, const clause& b);

    static bool isSubsetOf(const clause& a, const clause& b);

    static void insert(clause& cl, literal&& lit);

    static void insert(std::vector<clause>& cnf, clause&& cls);
};

}  // end of namespace souffle
