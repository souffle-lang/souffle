/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserUtils.cpp
 *
 * Defines class RuleBody to represents rule bodies.
 *
 ***********************************************************************/

#include "parser/ParserUtils.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
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
                cur.emplace_back(lit.cloneImpl());
            }
            for (const auto& lit : clauseB) {
                insert(cur, lit.cloneImpl());
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

VecOwn<ast::Clause> RuleBody::toClauseBodies() const {
    // collect clause results
    VecOwn<ast::Clause> bodies;
    for (const clause& cur : dnf) {
        bodies.push_back(mk<ast::Clause>(ast::QualifiedName::fromString("*")));
        ast::Clause& clause = *bodies.back();

        for (const literal& lit : cur) {
            // extract literal
            auto base = clone(lit.atom);
            // negate if necessary
            if (lit.negated) {
                // negate
                if (auto* atom = as<ast::Atom>(*base)) {
                    base.release();
                    base = mk<ast::Negation>(Own<ast::Atom>(atom), atom->getSrcLoc());
                } else if (auto* cstr = as<ast::Constraint>(*base)) {
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

RuleBody RuleBody::atom(Own<ast::Atom> atom) {
    RuleBody body;
    body.dnf.push_back(clause());
    body.dnf.back().emplace_back(literal{false, std::move(atom)});
    return body;
}

RuleBody RuleBody::constraint(Own<ast::Constraint> constraint) {
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

Own<ast::Atom> nameUnnamedVariables(Own<ast::Atom> atom) {
    assert(atom != nullptr && "Atom is a null-pointer");
    auto const& args = atom->getArguments();
    auto arity = atom->getArity();
    VecOwn<ast::Argument> newArgs;
    for (std::size_t i = 0; i < arity; i++) {
        if (isA<ast::UnnamedVariable>(args[i])) {
            std::stringstream varName;
            varName << "@var" << i;
            newArgs.push_back(mk<ast::Variable>(varName.str()));
        } else {
            newArgs.push_back(clone(args[i]));
        }
    }
    return mk<ast::Atom>(atom->getQualifiedName(), std::move(newArgs), atom->getSrcLoc());
}

}  // end of namespace souffle
