/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ResolveAliases.cpp
 *
 * Define classes and functionality related to the ResolveAliases
 * transformer.
 *
 ***********************************************************************/

#include "ast/transform/ResolveAliases.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Functor.h"
#include "ast/Literal.h"
#include "ast/Node.h"
#include "ast/Program.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/Variable.h"
#include "ast/analysis/Functor.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

namespace {

/**
 * A utility class for the unification process required to eliminate aliases.
 * A substitution maps variables to terms and can be applied as a transformation
 * to Arguments.
 */
class Substitution {
    // map type used for internally storing var->term mappings
    //      - note: variables are identified by their names
    using map_t = std::map<std::string, Own<Argument>>;

    // the mapping of variables to terms
    map_t varToTerm;

public:
    // -- Constructors/Destructors --

    Substitution() = default;

    Substitution(const std::string& var, const Argument* arg) {
        varToTerm.insert(std::make_pair(var, clone(arg)));
    }

    ~Substitution() = default;

    /**
     * Applies this substitution to the given argument and returns a pointer
     * to the modified argument.
     *
     * @param node the node to be transformed
     * @return a pointer to the modified or replaced node
     */
    Own<Node> operator()(Own<Node> node) const {
        // create a substitution mapper
        struct M : public NodeMapper {
            const map_t& map;

            M(const map_t& map) : map(map) {}

            using NodeMapper::operator();

            Own<Node> operator()(Own<Node> node) const override {
                // see whether it is a variable to be substituted
                if (auto var = as<ast::Variable>(node)) {
                    auto pos = map.find(var->getName());
                    if (pos != map.end()) {
                        return clone(pos->second);
                    }
                }

                // otherwise, apply the mapper recursively
                node->apply(*this);
                return node;
            }
        };

        // apply the mapper
        return M(varToTerm)(std::move(node));
    }

    /**
     * A generic, type consistent wrapper of the transformation operation above.
     */
    template <typename T>
    Own<T> operator()(Own<T> node) const {
        Own<Node> resPtr = (*this)(Own<Node>(node.release()));
        assert(isA<T>(resPtr) && "Invalid node type mapping.");
        return Own<T>(as<T>(resPtr.release()));
    }

    /**
     * Appends the given substitution s to this substitution t such that the
     * result t' is s composed with t (s o t).
     * i.e.,
     *      - if t(x) = y, then t'(x) = s(y)
     *      - if s(x) = y, and x is not mapped by t, then t'(x) = y
     */
    void append(const Substitution& sub) {
        // apply substitution on the rhs of all current mappings
        for (auto& pair : varToTerm) {
            pair.second = sub(std::move(pair.second));
        }

        // append unseen variables to the end
        for (const auto& pair : sub.varToTerm) {
            if (varToTerm.find(pair.first) == varToTerm.end()) {
                // not seen yet, add it in
                varToTerm.insert(std::make_pair(pair.first, clone(pair.second)));
            }
        }
    }

    /** A print function (for debugging) */
    void print(std::ostream& out) const {
        out << "{"
            << join(varToTerm, ",",
                       [](std::ostream& out, const std::pair<const std::string, Own<Argument>>& cur) {
                           out << cur.first << " -> " << *cur.second;
                       })
            << "}";
    }

    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& out, const Substitution& s) {
        s.print(out);
        return out;
    }
};

/**
 * An equality constraint between two Arguments utilised by the unification
 * algorithm required by the alias resolution.
 */
class Equation {
public:
    // the two terms to be equivalent
    Own<Argument> lhs;
    Own<Argument> rhs;

    Equation(const Argument& lhs, const Argument& rhs) : lhs(clone(lhs)), rhs(clone(rhs)) {}

    Equation(const Argument* lhs, const Argument* rhs) : lhs(clone(lhs)), rhs(clone(rhs)) {}

    Equation(const Equation& other) : lhs(clone(other.lhs)), rhs(clone(other.rhs)) {}

    Equation(Equation&& other) = default;

    ~Equation() = default;

    /**
     * Applies the given substitution to both sides of the equation.
     */
    void apply(const Substitution& sub) {
        lhs = sub(std::move(lhs));
        rhs = sub(std::move(rhs));
    }

    /**
     * Enables equations to be printed (for debugging)
     */
    void print(std::ostream& out) const {
        out << *lhs << " = " << *rhs;
    }

    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& out, const Equation& e) {
        e.print(out);
        return out;
    }
};

}  // namespace

Own<Clause> ResolveAliasesTransformer::resolveAliases(const Clause& clause) {
    // -- utilities --

    // tests whether something is a variable
    auto isVar = [&](const Argument& arg) { return isA<ast::Variable>(&arg); };

    // tests whether something is a record
    auto isRec = [&](const Argument& arg) { return isA<RecordInit>(&arg); };

    // tests whether something is a ADT
    auto isAdt = [&](const Argument& arg) { return isA<BranchInit>(&arg); };

    // tests whether something is a generator
    auto isGenerator = [&](const Argument& arg) {
        // aggregators
        if (isA<Aggregator>(&arg)) return true;

        // or multi-result functors
        const auto* inf = as<IntrinsicFunctor>(arg);
        if (inf == nullptr) return false;
        return analysis::FunctorAnalysis::isMultiResult(*inf);
    };

    // tests whether a value `a` occurs in a term `b`
    auto occurs = [](const Argument& a, const Argument& b) {
        bool res = false;
        visit(b, [&](const Argument& arg) { res = (res || (arg == a)); });
        return res;
    };

    // variables appearing as functorless arguments in atoms or records should not
    // be resolved
    std::set<std::string> baseGroundedVariables;
    for (const auto* atom : getBodyLiterals<Atom>(clause)) {
        for (const Argument* arg : atom->getArguments()) {
            if (const auto* var = as<ast::Variable>(arg)) {
                baseGroundedVariables.insert(var->getName());
            }
        }
        visit(*atom, [&](const RecordInit& rec) {
            for (const Argument* arg : rec.getArguments()) {
                if (const auto* var = as<ast::Variable>(arg)) {
                    baseGroundedVariables.insert(var->getName());
                }
            }
        });
        visit(*atom, [&](const BranchInit& adt) {
            for (const Argument* arg : adt.getArguments()) {
                if (const auto* var = as<ast::Variable>(arg)) {
                    baseGroundedVariables.insert(var->getName());
                }
            }
        });
    }

    // I) extract equations
    std::vector<Equation> equations;
    visit(clause, [&](const BinaryConstraint& constraint) {
        if (isEqConstraint(constraint.getBaseOperator())) {
            equations.push_back(Equation(constraint.getLHS(), constraint.getRHS()));
        }
    });

    // II) compute unifying substitution
    Substitution substitution;

    // a utility for processing newly identified mappings
    auto newMapping = [&](const std::string& var, const Argument* term) {
        // found a new substitution
        Substitution newMapping(var, term);

        // apply substitution to all remaining equations
        for (auto& equation : equations) {
            equation.apply(newMapping);
        }

        // add mapping v -> t to substitution
        substitution.append(newMapping);
    };

    while (!equations.empty()) {
        // get next equation to compute
        Equation equation = equations.back();
        equations.pop_back();

        // shortcuts for left/right
        const Argument& lhs = *equation.lhs;
        const Argument& rhs = *equation.rhs;

        // #1:  t = t   => skip
        if (lhs == rhs) {
            continue;
        }

        // #2:  [..] = [..]  => decompose
        if (isRec(lhs) && isRec(rhs)) {
            // get arguments
            const auto& lhs_args = static_cast<const RecordInit&>(lhs).getArguments();
            const auto& rhs_args = static_cast<const RecordInit&>(rhs).getArguments();

            // make sure sizes are identical
            assert(lhs_args.size() == rhs_args.size() && "Record lengths not equal");

            // create new equalities
            for (std::size_t i = 0; i < lhs_args.size(); i++) {
                equations.push_back(Equation(lhs_args[i], rhs_args[i]));
            }

            continue;
        }

        // #3:  neither is a variable    => skip
        if (!isVar(lhs) && !isVar(rhs)) {
            continue;
        }

        // #4:  v = w    => add mapping
        if (isVar(lhs) && isVar(rhs)) {
            auto& var = static_cast<const ast::Variable&>(lhs);
            newMapping(var.getName(), &rhs);
            continue;
        }

        // #5:  t = v   => swap
        if (!isVar(lhs)) {
            equations.push_back(Equation(rhs, lhs));
            continue;
        }

        // now we know lhs is a variable
        assert(isVar(lhs));

        // therefore, we have v = t
        const auto& v = static_cast<const ast::Variable&>(lhs);
        const Argument& t = rhs;

        // #6:  t is a generator => skip
        if (isGenerator(t)) {
            continue;
        }

        // #7:  v occurs in t   => skip
        if (occurs(v, t)) {
            continue;
        }

        assert(!occurs(v, t));

        // #8:  t is a record   => add mapping
        if (isRec(t) || isAdt(t)) {
            newMapping(v.getName(), &t);
            continue;
        }

        // #9:  v is already grounded   => skip
        auto pos = baseGroundedVariables.find(v.getName());
        if (pos != baseGroundedVariables.end()) {
            continue;
        }

        // add new mapping
        newMapping(v.getName(), &t);
    }

    // III) compute resulting clause
    return substitution(clone(clause));
}

Own<Clause> ResolveAliasesTransformer::removeTrivialEquality(const Clause& clause) {
    auto res = Own<Clause>(clause.cloneHead());

    // add all literals, except filtering out t = t constraints
    for (Literal* literal : clause.getBodyLiterals()) {
        if (auto* constraint = as<BinaryConstraint>(literal)) {
            // TODO: don't filter out `FEQ` constraints, since `x = x` can fail when `x` is a NaN
            if (isEqConstraint(constraint->getBaseOperator())) {
                if (*constraint->getLHS() == *constraint->getRHS()) {
                    continue;  // skip this one
                }
            }
        }

        res->addToBody(clone(literal));
    }

    // done
    return res;
}

Own<Clause> ResolveAliasesTransformer::removeComplexTermsInAtoms(const Clause& clause) {
    Own<Clause> res(clone(clause));

    // get list of atoms
    std::vector<Atom*> atoms = getBodyLiterals<Atom>(*res);

    // find all functors in atoms
    std::vector<const Argument*> terms;
    for (const Atom* atom : atoms) {
        for (const Argument* arg : atom->getArguments()) {
            // ignore if not a functor
            if (!isA<Functor>(arg) && !isA<TypeCast>(arg)) {
                continue;
            }

            // add this functor if not seen yet
            if (!any_of(terms, [&](const Argument* cur) { return *cur == *arg; })) {
                terms.push_back(arg);
            }
        }
    }

    // find all functors in records/ADTs too
    visit(atoms, [&](const RecordInit& rec) {
        for (const Argument* arg : rec.getArguments()) {
            // ignore if not a functor
            if (!isA<Functor>(arg)) {
                continue;
            }

            // add this functor if not seen yet
            if (!any_of(terms, [&](const Argument* cur) { return *cur == *arg; })) {
                terms.push_back(arg);
            }
        }
    });
    visit(atoms, [&](const BranchInit& adt) {
        for (const Argument* arg : adt.getArguments()) {
            // ignore if not a functor
            if (!isA<Functor>(arg)) {
                continue;
            }

            // add this functor if not seen yet
            if (!any_of(terms, [&](const Argument* cur) { return *cur == *arg; })) {
                terms.push_back(arg);
            }
        }
    });

    // substitute them with new variables (a real map would compare pointers)
    using substitution_map = std::vector<std::pair<Own<Argument>, Own<ast::Variable>>>;
    substitution_map termToVar;

    static int varCounter = 0;
    for (const Argument* arg : terms) {
        // create a new mapping for this term
        auto term = clone(arg);
        auto newVariable = mk<ast::Variable>(" _tmp_" + toString(varCounter++));
        termToVar.push_back(std::make_pair(std::move(term), std::move(newVariable)));
    }

    // apply mapping to replace the terms with the variables
    struct Update : public NodeMapper {
        const substitution_map& map;

        Update(const substitution_map& map) : map(map) {}

        Own<Node> operator()(Own<Node> node) const override {
            // check whether node needs to be replaced
            for (const auto& pair : map) {
                auto& term = pair.first;
                auto& variable = pair.second;

                if (*term == *node) {
                    return clone(variable);
                }
            }

            // continue recursively
            node->apply(*this);
            return node;
        }
    };

    // update atoms
    Update update(termToVar);
    for (Atom* atom : atoms) {
        atom->apply(update);
    }

    // add the necessary variable constraints to the clause
    for (const auto& pair : termToVar) {
        auto& term = pair.first;
        auto& variable = pair.second;

        res->addToBody(mk<BinaryConstraint>(BinaryConstraintOp::EQ, clone(variable), clone(term)));
    }

    return res;
}

bool ResolveAliasesTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    // get all clauses
    std::vector<const Clause*> clauses;
    visit(program, [&](const Relation& rel) {
        const auto& qualifiers = rel.getQualifiers();
        // Don't resolve clauses of inlined relations
        if (qualifiers.count(RelationQualifier::INLINE) == 0) {
            for (auto&& clause : program.getClauses(rel)) {
                clauses.push_back(clause);
            }
        }
    });

    // clean all clauses
    for (const Clause* clause : clauses) {
        // -- Step 1 --
        // get rid of aliases
        Own<Clause> noAlias = resolveAliases(*clause);

        // clean up equalities
        Own<Clause> cleaned = removeTrivialEquality(*noAlias);

        // -- Step 2 --
        // restore simple terms in atoms
        Own<Clause> normalised = removeComplexTermsInAtoms(*cleaned);

        // swap if changed
        if (*normalised != *clause) {
            changed = true;
            program.removeClause(*clause);
            program.addClause(std::move(normalised));
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
