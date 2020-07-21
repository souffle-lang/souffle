/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstGroundAnalysis.cpp
 *
 * Implements AST Analysis methods to find the grounded arguments in a clause
 *
 ***********************************************************************/

#include "ast/analysis/AstGroundAnalysis.h"
#include "BinaryConstraintOps.h"
#include "Constraints.h"
#include "PrecedenceGraph.h"
#include "RelationTag.h"
#include "ast/AstArgument.h"
#include "ast/AstClause.h"
#include "ast/AstLiteral.h"
#include "ast/AstTranslationUnit.h"
#include "ast/AstVisitor.h"
#include "ast/analysis/AstConstraintAnalysis.h"
#include "ast/analysis/AstGroundAnalysis.h"
#include "utility/StreamUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle {

namespace {

// -----------------------------------------------------------------------------
//                        Boolean Disjunct Lattice
// -----------------------------------------------------------------------------

/**
 * The disjunct meet operator, aka boolean or.
 */
struct bool_or {
    bool operator()(bool& a, bool b) const {
        bool t = a;
        a = a || b;
        return t != a;
    }
};

/**
 * A factory producing the value false.
 */
struct false_factory {
    bool operator()() const {
        return false;
    }
};

/**
 * The definition of a lattice utilizing the boolean values {true} and {false} as
 * its value set and the || operation as its meet operation. Correspondingly,
 * the bottom value is {false} and the top value {true}.
 */
struct bool_disjunct_lattic : public property_space<bool, bool_or, false_factory> {};

/** A base type for analysis based on the boolean disjunct lattice */
using BoolDisjunctVar = AstConstraintAnalysisVar<bool_disjunct_lattic>;

/** A base type for constraints on the boolean disjunct lattice */
using BoolDisjunctConstraint = std::shared_ptr<Constraint<BoolDisjunctVar>>;

/**
 * A constraint factory for a constraint ensuring that the value assigned to the
 * given variable is (at least) {true}
 */
BoolDisjunctConstraint isTrue(const BoolDisjunctVar& var) {
    struct C : public Constraint<BoolDisjunctVar> {
        BoolDisjunctVar var;
        C(BoolDisjunctVar var) : var(std::move(var)) {}
        bool update(Assignment<BoolDisjunctVar>& ass) const override {
            auto res = !ass[var];
            ass[var] = true;
            return res;
        }
        void print(std::ostream& out) const override {
            out << var << " is true";
        }
    };
    return std::make_shared<C>(var);
}

/**
 * A constraint factory for a constraint ensuring the constraint
 *
 *                              a ⇒ b
 *
 * Hence, whenever a is mapped to {true}, so is b.
 */
BoolDisjunctConstraint imply(const BoolDisjunctVar& a, const BoolDisjunctVar& b) {
    return sub(a, b, "⇒");
}

/**
 * A constraint factory for a constraint ensuring the constraint
 *
 *               vars[0] ∧ vars[1] ∧ ... ∧ vars[n] ⇒ res
 *
 * Hence, whenever all variables vars[i] are mapped to true, so is res.
 */
BoolDisjunctConstraint imply(const std::vector<BoolDisjunctVar>& vars, const BoolDisjunctVar& res) {
    struct C : public Constraint<BoolDisjunctVar> {
        BoolDisjunctVar res;
        std::vector<BoolDisjunctVar> vars;

        C(BoolDisjunctVar res, std::vector<BoolDisjunctVar> vars)
                : res(std::move(res)), vars(std::move(vars)) {}

        bool update(Assignment<BoolDisjunctVar>& ass) const override {
            bool r = ass[res];
            if (r) {
                return false;
            }

            for (const auto& cur : vars) {
                if (!ass[cur]) {
                    return false;
                }
            }

            ass[res] = true;
            return true;
        }

        void print(std::ostream& out) const override {
            out << join(vars, " ∧ ") << " ⇒ " << res;
        }
    };

    return std::make_shared<C>(res, vars);
}
}  // namespace

/***
 * computes for variables in the clause whether they are grounded
 */

std::map<const AstArgument*, bool> getGroundedTerms(const AstTranslationUnit& tu, const AstClause& clause) {
    struct Analysis : public AstConstraintAnalysis<BoolDisjunctVar> {
        const RelationDetailCache& relCache;
        std::set<const AstAtom*> ignore;

        Analysis(const AstTranslationUnit& tu) : relCache(*tu.getAnalysis<RelationDetailCache>()) {}

        // atoms are producing grounded variables
        void visitAtom(const AstAtom& cur) override {
            // some atoms need to be skipped (head or negation)
            if (ignore.find(&cur) != ignore.end()) {
                return;
            }

            // all arguments are grounded
            for (const auto& arg : cur.getArguments()) {
                addConstraint(isTrue(getVar(arg)));
            }
        }

        // negations need to be skipped
        void visitNegation(const AstNegation& cur) override {
            // add nested atom to black-list
            ignore.insert(cur.getAtom());
        }

        // also skip head if we don't have an inline qualifier
        void visitClause(const AstClause& clause) override {
            if (auto clauseHead = clause.getHead()) {
                auto relation = relCache.getRelation(clauseHead->getQualifiedName());
                // Only skip the head if the relation ISN'T inline. Keeping the head will ground
                // any mentioned variables, allowing us to pretend they're grounded.
                if (!(relation && relation->hasQualifier(RelationQualifier::INLINE))) {
                    ignore.insert(clauseHead);
                }
            }
        }

        // binary equality relations propagates groundness
        void visitBinaryConstraint(const AstBinaryConstraint& cur) override {
            // only target equality
            if (!isEqConstraint(cur.getOperator())) {
                return;
            }

            // if equal, link right and left side
            auto lhs = getVar(cur.getLHS());
            auto rhs = getVar(cur.getRHS());

            addConstraint(imply(lhs, rhs));
            addConstraint(imply(rhs, lhs));
        }

        // record init nodes
        void visitRecordInit(const AstRecordInit& init) override {
            auto cur = getVar(init);

            std::vector<BoolDisjunctVar> vars;

            // if record is grounded, so are all its arguments
            for (const auto& arg : init.getArguments()) {
                auto arg_var = getVar(arg);
                addConstraint(imply(cur, arg_var));
                vars.push_back(arg_var);
            }

            // if all arguments are grounded, so is the record
            addConstraint(imply(vars, cur));
        }

        // constants are also sources of grounded values
        void visitConstant(const AstConstant& c) override {
            addConstraint(isTrue(getVar(c)));
        }

        // aggregators are grounding values
        void visitAggregator(const AstAggregator& c) override {
            addConstraint(isTrue(getVar(c)));
        }

        // functors with grounded values are grounded values
        void visitFunctor(const AstFunctor& cur) override {
            auto fun = getVar(cur);
            std::vector<BoolDisjunctVar> varArgs;
            for (const auto& arg : cur.getArguments()) {
                varArgs.push_back(getVar(arg));
            }
            addConstraint(imply(varArgs, fun));
        }

        // casts propogate groundedness in and out
        void visitTypeCast(const AstTypeCast& cast) override {
            addConstraint(imply(getVar(cast.getValue()), getVar(cast)));
        }
    };

    // run analysis on given clause
    return Analysis(tu).analyse(clause);
}

}  // end of namespace souffle
