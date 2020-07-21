/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstParserUtils.cpp
 *
 * Defines class RuleBody to represents rule bodies.
 *
 ***********************************************************************/

#include "ast/AstParserUtils.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstNode.h"
#include "ast/AstUtils.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle {

RuleBody RuleBody::negated() const {
    RuleBody res = getTrue();

    for (const clause& cur : dnf) {
        RuleBody step;
        for (const literal& lit : cur) {
            step.dnf.push_back(clause());
            step.dnf.back().emplace_back(literal{!lit.negated, clone(lit.atom)});
        }

        res.conjunct(std::move(step));
    }

    return res;
}

void RuleBody::conjunct(RuleBody other) {
    // avoid making clones if possible
    if (dnf.size() == 1 && other.dnf.size() == 1) {
        for (auto&& rhs : other.dnf[0]) {
            insert(dnf[0], std::move(rhs));
        }

        return;
    }

    // compute the product of the disjunctions
    std::vector<clause> res;

    for (const auto& clauseA : dnf) {
        for (const auto& clauseB : other.dnf) {
            clause cur;

            for (const auto& lit : clauseA) {
                cur.emplace_back(lit.clone());
            }
            for (const auto& lit : clauseB) {
                insert(cur, lit.clone());
            }

            insert(res, std::move(cur));
        }
    }

    dnf = std::move(res);
}

void RuleBody::disjunct(RuleBody other) {
    // append the clauses of the other body to this body
    for (auto& cur : other.dnf) {
        insert(dnf, std::move(cur));
    }
}

VecOwn<AstClause> RuleBody::toClauseBodies() const {
    // collect clause results
    VecOwn<AstClause> bodies;
    for (const clause& cur : dnf) {
        bodies.push_back(mk<AstClause>());
        AstClause& clause = *bodies.back();

        for (const literal& lit : cur) {
            // extract literal
            auto base = clone(lit.atom);
            // negate if necessary
            if (lit.negated) {
                // negate
                if (auto* atom = dynamic_cast<AstAtom*>(&*base)) {
                    base.release();
                    base = mk<AstNegation>(std::unique_ptr<AstAtom>(atom));
                    base->setSrcLoc(atom->getSrcLoc());
                } else if (auto* cstr = dynamic_cast<AstConstraint*>(&*base)) {
                    negateConstraintInPlace(*cstr);
                }
            }

            // add to result
            clause.addToBody(std::move(base));
        }
    }

    // done
    return bodies;
}

// -- factory functions --

RuleBody RuleBody::getTrue() {
    RuleBody body;
    body.dnf.push_back(clause());
    return body;
}

RuleBody RuleBody::getFalse() {
    return RuleBody();
}

RuleBody RuleBody::atom(Own<AstAtom> atom) {
    RuleBody body;
    body.dnf.push_back(clause());
    body.dnf.back().emplace_back(literal{false, std::move(atom)});
    return body;
}

RuleBody RuleBody::constraint(Own<AstConstraint> constraint) {
    RuleBody body;
    body.dnf.push_back(clause());
    body.dnf.back().emplace_back(literal{false, std::move(constraint)});
    return body;
}

std::ostream& operator<<(std::ostream& out, const RuleBody& body) {
    return out << join(body.dnf, ";", [](std::ostream& out, const RuleBody::clause& cur) {
        out << join(cur, ",", [](std::ostream& out, const RuleBody::literal& l) {
            if (l.negated) {
                out << "!";
            }
            out << *l.atom;
        });
    });
}

bool RuleBody::equal(const literal& a, const literal& b) {
    return a.negated == b.negated && *a.atom == *b.atom;
}

bool RuleBody::equal(const clause& a, const clause& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const auto& i : a) {
        bool found = false;
        for (const auto& j : b) {
            if (equal(i, j)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool RuleBody::isSubsetOf(const clause& a, const clause& b) {
    if (a.size() > b.size()) {
        return false;
    }
    for (const auto& i : a) {
        bool found = false;
        for (const auto& j : b) {
            if (equal(i, j)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

void RuleBody::insert(clause& cl, literal&& lit) {
    for (const auto& cur : cl) {
        if (equal(cur, lit)) {
            return;
        }
    }
    cl.emplace_back(std::move(lit));
}

void RuleBody::insert(std::vector<clause>& cnf, clause&& cls) {
    for (const auto& cur : cnf) {
        if (isSubsetOf(cur, cls)) {
            return;
        }
    }
    std::vector<clause> res;
    for (auto& cur : cnf) {
        if (!isSubsetOf(cls, cur)) {
            res.push_back(std::move(cur));
        }
    }
    res.swap(cnf);
    cnf.push_back(std::move(cls));
}

}  // end of namespace souffle
