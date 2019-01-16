/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ResolveAliasesTransformer.cpp
 *
 * Define classes and functionality related to the ResolveAliases
 * transformer.
 *
 ***********************************************************************/

#include "AstTransforms.h"
#include "AstVisitor.h"

namespace souffle {

namespace {

/**
 * A utility class for the unification process required to eliminate aliases.
 * A substitution maps variables to terms and can be applied as a transformation
 * to AstArguments.
 */
class Substitution {
    // map type used for internally storing var->term mappings
    //      - note: variables are identified by their names
    using map_t = std::map<std::string, std::unique_ptr<AstArgument>>;

    // the mapping of variables to terms
    map_t varToTerm;

public:
    // -- Constructors/Destructors --

    Substitution() = default;

    Substitution(const std::string& var, const AstArgument* arg) {
        varToTerm.insert(std::make_pair(var, std::unique_ptr<AstArgument>(arg->clone())));
    }

    ~Substitution() = default;

    /**
     * Applies this substitution to the given argument and returns a pointer
     * to the modified argument.
     *
     * @param node the node to be transformed
     * @return a pointer to the modified or replaced node
     */
    std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const {
        // create a substitution mapper
        struct M : public AstNodeMapper {
            const map_t& map;

            M(const map_t& map) : map(map) {}

            using AstNodeMapper::operator();

            std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
                // see whether it is a variable to be substituted
                if (auto var = dynamic_cast<AstVariable*>(node.get())) {
                    auto pos = map.find(var->getName());
                    if (pos != map.end()) {
                        return std::unique_ptr<AstNode>(pos->second->clone());
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
    std::unique_ptr<T> operator()(std::unique_ptr<T> node) const {
        std::unique_ptr<AstNode> resPtr = (*this)(std::unique_ptr<AstNode>(node.release()));
        assert(nullptr != dynamic_cast<T*>(resPtr.get()) && "Invalid node type mapping.");
        return std::unique_ptr<T>(dynamic_cast<T*>(resPtr.release()));
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
                varToTerm.insert(
                        std::make_pair(pair.first, std::unique_ptr<AstArgument>(pair.second->clone())));
            }
        }
    }

    /** A print function (for debugging) */
    void print(std::ostream& out) const {
        out << "{"
            << join(varToTerm, ",",
                       [](std::ostream& out,
                               const std::pair<const std::string, std::unique_ptr<AstArgument>>& cur) {
                           out << cur.first << " -> " << *cur.second;
                       })
            << "}";
    }

    friend std::ostream& operator<<(std::ostream& out, const Substitution& s) __attribute__((unused)) {
        s.print(out);
        return out;
    }
};

/**
 * An equality constraint between two AstArguments utilised by the unification
 * algorithm required by the alias resolution.
 */
class Equation {
public:
    // the two terms to be equivalent
    std::unique_ptr<AstArgument> lhs;
    std::unique_ptr<AstArgument> rhs;

    Equation(const AstArgument& lhs, const AstArgument& rhs)
            : lhs(std::unique_ptr<AstArgument>(lhs.clone())), rhs(std::unique_ptr<AstArgument>(rhs.clone())) {
    }

    Equation(const AstArgument* lhs, const AstArgument* rhs)
            : lhs(std::unique_ptr<AstArgument>(lhs->clone())),
              rhs(std::unique_ptr<AstArgument>(rhs->clone())) {}

    Equation(const Equation& other)
            : lhs(std::unique_ptr<AstArgument>(other.lhs->clone())),
              rhs(std::unique_ptr<AstArgument>(other.rhs->clone())) {}

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

    friend std::ostream& operator<<(std::ostream& out, const Equation& e) __attribute__((unused)) {
        e.print(out);
        return out;
    }
};

}  // namespace

std::unique_ptr<AstClause> ResolveAliasesTransformer::resolveAliases(const AstClause& clause) {
    // -- utilities --

    // tests whether something is a variable
    auto isVar = [&](const AstArgument& arg) { return dynamic_cast<const AstVariable*>(&arg); };

    // tests whether something is a record
    auto isRec = [&](const AstArgument& arg) { return dynamic_cast<const AstRecordInit*>(&arg); };

    // tests whether a value `a` occurs in a term `b`
    auto occurs = [](const AstArgument& a, const AstArgument& b) {
        bool res = false;
        visitDepthFirst(b, [&](const AstArgument& arg) { res = res || (arg == a); });
        return res;
    };

    // find all variables appearing as functorless arguments in grounding atoms
    // these variables are the source of groundedness
    // e.g. a(y) :- b(x), y = x + 1. -- y is only grounded because x appears in b(x)
    std::set<std::string> baseGroundedVariables;
    for (const AstAtom* atom : clause.getAtoms()) {
        for (const AstArgument* arg : atom->getArguments()) {
            if (const AstVariable* var = dynamic_cast<const AstVariable*>(arg)) {
                baseGroundedVariables.insert(var->getName());
            }
        }
    }

    // I) extract equations
    std::vector<Equation> equations;
    visitDepthFirst(clause, [&](const AstBinaryConstraint& constraint) {
        if (constraint.getOperator() == BinaryConstraintOp::EQ) {
            equations.push_back(Equation(constraint.getLHS(), constraint.getRHS()));
        }
    });

    // II) compute unifying substitution
    Substitution substitution;

    // a utility for processing newly identified mappings
    auto newMapping = [&](const std::string& var, const AstArgument* term) {
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
        const AstArgument& lhs = *equation.lhs;
        const AstArgument& rhs = *equation.rhs;

        // #1:  t = t   => skip
        if (lhs == rhs) {
            continue;
        }

        // #2:  [..] = [..]  => decompose
        if (isRec(lhs) && isRec(rhs)) {
            // get arguments
            const auto& lhs_args = static_cast<const AstRecordInit&>(lhs).getArguments();
            const auto& rhs_args = static_cast<const AstRecordInit&>(rhs).getArguments();

            // make sure sizes are identical
            assert(lhs_args.size() == rhs_args.size() && "Record lengths not equal");

            // create new equalities
            for (size_t i = 0; i < lhs_args.size(); i++) {
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
            auto& var = static_cast<const AstVariable&>(lhs);
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
        const auto& v = static_cast<const AstVariable&>(lhs);
        const AstArgument& t = rhs;

        // #6:  v occurs in t   => skip
        if (occurs(v, t)) {
            continue;
        }

        assert(!occurs(v, t));

        // #7:  t is a record   => add mapping
        if (isRec(t)) {
            newMapping(v.getName(), &t);
            continue;
        }

        // #8:  v is already grounded   => skip
        auto pos = baseGroundedVariables.find(v.getName());
        if (pos != baseGroundedVariables.end()) {
            continue;
        }

        // add new mapping
        newMapping(v.getName(), &t);
    }

    // III) compute resulting clause
    return substitution(std::unique_ptr<AstClause>(clause.clone()));
}

std::unique_ptr<AstClause> ResolveAliasesTransformer::removeTrivialEquality(const AstClause& clause) {
    std::unique_ptr<AstClause> res(clause.cloneHead());

    // add all literals, except filtering out t = t constraints
    for (AstLiteral* literal : clause.getBodyLiterals()) {
        if (auto* constraint = dynamic_cast<AstBinaryConstraint*>(literal)) {
            if (constraint->getOperator() == BinaryConstraintOp::EQ) {
                if (*constraint->getLHS() == *constraint->getRHS()) {
                    continue;  // skip this one
                }
            }
        }

        res->addToBody(std::unique_ptr<AstLiteral>(literal->clone()));
    }

    // done
    return res;
}

std::unique_ptr<AstClause> ResolveAliasesTransformer::removeComplexTermsInAtoms(const AstClause& clause) {
    std::unique_ptr<AstClause> res(clause.clone());

    // get list of atoms
    std::vector<AstAtom*> atoms = res->getAtoms();

    // find all functors in atoms
    std::vector<const AstArgument*> terms;
    for (const AstAtom* atom : atoms) {
        for (const AstArgument* arg : atom->getArguments()) {
            // ignore if not a functor
            if (dynamic_cast<const AstFunctor*>(arg) == nullptr) {
                continue;
            }

            // add this functor if not seen yet
            if (!any_of(terms, [&](const AstArgument* cur) { return *cur == *arg; })) {
                terms.push_back(arg);
            }
        }
    }

    // substitute them with new variables (a real map would compare pointers)
    using substitution_map =
            std::vector<std::pair<std::unique_ptr<AstArgument>, std::unique_ptr<AstVariable>>>;
    substitution_map termToVar;

    int varCounter = 0;
    for (const AstArgument* arg : terms) {
        // create a new mapping for this term
        auto term = std::unique_ptr<AstArgument>(arg->clone());
        auto newVariable = std::make_unique<AstVariable>(" _tmp_" + toString(varCounter++));
        termToVar.push_back(std::make_pair(std::move(term), std::move(newVariable)));
    }

    // apply mapping to replace the terms with the variables
    struct Update : public AstNodeMapper {
        const substitution_map& map;

        Update(const substitution_map& map) : map(map) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // check whether node needs to be replaced
            for (const auto& pair : map) {
                auto& term = pair.first;
                auto& variable = pair.second;

                if (*term == *node) {
                    return std::unique_ptr<AstNode>(variable->clone());
                }
            }

            // continue recursively
            node->apply(*this);
            return node;
        }
    };

    // update atoms
    Update update(termToVar);
    for (AstAtom* atom : atoms) {
        atom->apply(update);
    }

    // add the necessary variable constraints to the clause
    for (const auto& pair : termToVar) {
        auto& term = pair.first;
        auto& variable = pair.second;

        res->addToBody(std::make_unique<AstBinaryConstraint>(BinaryConstraintOp::EQ,
                std::unique_ptr<AstArgument>(variable->clone()),
                std::unique_ptr<AstArgument>(term->clone())));
    }

    return res;
}

bool ResolveAliasesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    AstProgram& program = *translationUnit.getProgram();

    // get all clauses
    std::vector<const AstClause*> clauses;
    visitDepthFirst(program, [&](const AstRelation& rel) {
        for (const auto& clause : rel.getClauses()) {
            clauses.push_back(clause);
        }
    });

    // clean all clauses
    for (const AstClause* clause : clauses) {
        // -- Step 1 --
        // get rid of aliases
        std::unique_ptr<AstClause> noAlias = resolveAliases(*clause);

        // clean up equalities
        std::unique_ptr<AstClause> cleaned = removeTrivialEquality(*noAlias);

        // -- Step 2 --
        // restore simple terms in atoms
        std::unique_ptr<AstClause> normalised = removeComplexTermsInAtoms(*cleaned);

        // swap if changed
        if (*normalised != *clause) {
            changed = true;
            program.removeClause(clause);
            program.appendClause(std::move(normalised));
        }
    }

    return changed;
}

}  // namespace souffle
