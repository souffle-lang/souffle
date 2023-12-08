/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Ground.cpp
 *
 * Implements AST Analysis methods to find the grounded arguments in a clause
 *
 ***********************************************************************/

#include "ast/analysis/Ground.h"
#include "RelationTag.h"
#include "ast/Aggregator.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/BranchInit.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/Functor.h"
#include "ast/Negation.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/TypeCast.h"
#include "ast/analysis/Constraint.h"
#include "ast/analysis/ConstraintSystem.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/StreamUtil.h"
#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace souffle::ast::analysis {

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
using BoolDisjunctVar = ConstraintAnalysisVar<bool_disjunct_lattic>;

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

struct GroundednessAnalysis : public ConstraintAnalysis<BoolDisjunctVar> {
    Program& program;
    std::set<const Atom*> ignore;
    UnorderedQualifiedNameMap<std::set<std::size_t>> latticeAttributes;
    bool isLatticeTransformerPass;

    GroundednessAnalysis(const TranslationUnit& tu, bool isLatticeTransformerPass)
            : program(tu.getProgram()), isLatticeTransformerPass(isLatticeTransformerPass) {
        if (isLatticeTransformerPass) {
            for (const Relation* rel : program.getRelations()) {
                const auto attributes = rel->getAttributes();
                const auto& name = rel->getQualifiedName();
                for (std::size_t i = 0; i < attributes.size(); i++) {
                    if (attributes[i]->getIsLattice()) {
                        latticeAttributes[name].insert(i);
                    }
                }
            }
        }
    }

    // atoms are producing grounded variables
    void visit_(type_identity<Atom>, const Atom& cur) override {
        // some atoms need to be skipped (head or negation)
        if (ignore.find(&cur) != ignore.end()) {
            return;
        }

        // all arguments are grounded except lattice arguments
        const auto& name = cur.getQualifiedName();
        const auto& args = cur.getArguments();
        for (std::size_t i = 0; i < cur.getArity(); i++) {
            if (!isLatticeTransformerPass || !latticeAttributes.count(name) ||
                    !latticeAttributes[name].count(i)) {
                addConstraint(isTrue(getVar(args[i])));
            }
        }
    }

    // negations need to be skipped
    void visit_(type_identity<Negation>, const Negation& cur) override {
        // add nested atom to black-list
        ignore.insert(cur.getAtom());
    }

    // also skip head if we don't have an inline qualifier
    void visit_(type_identity<Clause>, const Clause& clause) override {
        auto relation = program.getRelation(clause);
        // Only skip the head if the relation ISN'T inline. Keeping the head will ground
        // any mentioned variables, allowing us to pretend they're grounded.
        if (!(relation && relation->hasQualifier(RelationQualifier::INLINE))) {
            ignore.insert(clause.getHead());
        }
    }

    // binary equality relations propagates groundness
    void visit_(type_identity<BinaryConstraint>, const BinaryConstraint& cur) override {
        // only target equality
        if (!isEqConstraint(cur.getBaseOperator())) {
            return;
        }

        // if equal, link right and left side
        auto lhs = getVar(cur.getLHS());
        auto rhs = getVar(cur.getRHS());

        addConstraint(imply(lhs, rhs));
        addConstraint(imply(rhs, lhs));
    }

    // record init nodes
    void visit_(type_identity<RecordInit>, const RecordInit& init) override {
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

    void visit_(type_identity<BranchInit>, const BranchInit& adt) override {
        auto branchVar = getVar(adt);

        std::vector<BoolDisjunctVar> argVars;

        // If the branch is grounded so are its arguments.
        for (const auto* arg : adt.getArguments()) {
            auto argVar = getVar(arg);
            addConstraint(imply(branchVar, argVar));
            argVars.push_back(argVar);
        }

        // if all arguments are grounded so is the branch.
        addConstraint(imply(argVars, branchVar));
    }

    // Constants are also sources of grounded values
    void visit_(type_identity<Constant>, const Constant& constant) override {
        addConstraint(isTrue(getVar(constant)));
    }

    // Aggregators are grounding values
    void visit_(type_identity<Aggregator>, const Aggregator& aggregator) override {
        addConstraint(isTrue(getVar(aggregator)));
    }

    // Functors with grounded values are grounded values
    void visit_(type_identity<Functor>, const Functor& functor) override {
        auto var = getVar(functor);
        std::vector<BoolDisjunctVar> varArgs;
        for (const auto& arg : functor.getArguments()) {
            varArgs.push_back(getVar(arg));
        }
        addConstraint(imply(varArgs, var));
    }

    // casts propogate groundedness in and out
    void visit_(type_identity<TypeCast>, const ast::TypeCast& cast) override {
        addConstraint(imply(getVar(cast.getValue()), getVar(cast)));
    }
};

}  // namespace

/***
 * computes for variables in the clause whether they are grounded
 */
std::map<const Argument*, bool> getGroundedTerms(
        const TranslationUnit& tu, const Clause& clause, bool isLatticeTransformerPass) {
    // run analysis on given clause
    return GroundednessAnalysis(tu, isLatticeTransformerPass).analyse(clause);
}

}  // namespace souffle::ast::analysis
