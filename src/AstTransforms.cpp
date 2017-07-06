/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTransforms.cpp
 *
 * Implementation of AST transformation passes.
 *
 ***********************************************************************/

#include "AstTransforms.h"
#include "AstTypeAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "MagicSet.h"
#include "PrecedenceGraph.h"

namespace souffle {

void ResolveAliasesTransformer::resolveAliases(AstProgram& program) {
    // get all clauses
    std::vector<const AstClause*> clauses;
    visitDepthFirst(program, [&](const AstRelation& rel) {
        for (const auto& cur : rel.getClauses()) {
            clauses.push_back(cur);
        }
    });

    // clean all clauses
    for (const AstClause* cur : clauses) {
        // -- Step 1 --
        // get rid of aliases
        std::unique_ptr<AstClause> noAlias = resolveAliases(*cur);

        // clean up equalities
        std::unique_ptr<AstClause> cleaned = removeTrivialEquality(*noAlias);

        // -- Step 2 --
        // restore simple terms in atoms
        removeComplexTermsInAtoms(*cleaned);

        // exchange rule
        program.removeClause(cur);
        program.appendClause(std::move(cleaned));
    }
}

namespace {

/**
 * A utility class for the unification process required to eliminate
 * aliases. A substitution maps variables to terms and can be applied
 * as a transformation to AstArguments.
 */
class Substitution {
    // the type of map for storing mappings internally
    //   - variables are identified by their name (!)
    typedef std::map<std::string, std::unique_ptr<AstArgument>> map_t;

    /** The mapping of variables to terms (see type def above) */
    map_t map;

public:
    // -- Ctors / Dtors --

    Substitution(){};

    Substitution(const std::string& var, const AstArgument* arg) {
        map.insert(std::make_pair(var, std::unique_ptr<AstArgument>(arg->clone())));
    }

    virtual ~Substitution() = default;

    /**
     * Applies this substitution to the given argument and
     * returns a pointer to the modified argument.
     *
     * @param node the node to be transformed
     * @return a pointer to the modified or replaced node
     */
    virtual std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const {
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

                // otherwise - apply mapper recursively
                node->apply(*this);
                return node;
            }
        };

        // apply mapper
        return M(map)(std::move(node));
    }

    /**
     * A generic, type consistent wrapper of the transformation
     * operation above.
     */
    template <typename T>
    std::unique_ptr<T> operator()(std::unique_ptr<T> node) const {
        std::unique_ptr<AstNode> resPtr =
                (*this)(std::unique_ptr<AstNode>(static_cast<AstNode*>(node.release())));
        assert(dynamic_cast<T*>(resPtr.get()) && "Invalid node type mapping.");
        return std::unique_ptr<T>(dynamic_cast<T*>(resPtr.release()));
    }

    /**
     * Appends the given substitution to this substitution such that
     * this substitution has the same effect as applying this following
     * the given substitution in sequence.
     */
    void append(const Substitution& s) {
        // apply substitution on all current mappings
        for (auto& cur : map) {
            cur.second = s(std::move(cur.second));
        }

        // append uncovered variables to the end
        for (const auto& cur : s.map) {
            auto pos = map.find(cur.first);
            if (pos != map.end()) {
                continue;
            }
            map.insert(std::make_pair(cur.first, std::unique_ptr<AstArgument>(cur.second->clone())));
        }
    }

    /** A print function (for debugging) */
    void print(std::ostream& out) const {
        out << "{" << join(map, ",",
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
 * An equality constraint between to AstArguments utilized by the
 * unification algorithm required by the alias resolution.
 */
struct Equation {
    /** The two terms to be equivalent */
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

    Equation(Equation&& other) : lhs(std::move(other.lhs)), rhs(std::move(other.rhs)) {}

    ~Equation() = default;

    /**
     * Applies the given substitution to both sides of the equation.
     */
    void apply(const Substitution& s) {
        lhs = s(std::move(lhs));
        rhs = s(std::move(rhs));
    }

    /** Enables equations to be printed (for debugging) */
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
    /**
     * This alias analysis utilizes unification over the equality
     * constraints in clauses.
     */

    // -- utilities --

    // tests whether something is a ungrounded variable
    auto isVar = [&](const AstArgument& arg) { return dynamic_cast<const AstVariable*>(&arg); };

    // tests whether something is a record
    auto isRec = [&](const AstArgument& arg) { return dynamic_cast<const AstRecordInit*>(&arg); };

    // tests whether a value a occurs in a term b
    auto occurs = [](const AstArgument& a, const AstArgument& b) {
        bool res = false;
        visitDepthFirst(b, [&](const AstArgument& cur) { res = res || cur == a; });
        return res;
    };

    // I) extract equations
    std::vector<Equation> equations;
    visitDepthFirst(clause, [&](const AstConstraint& rel) {
        if (rel.getOperator() == BinaryConstraintOp::EQ) {
            equations.push_back(Equation(rel.getLHS(), rel.getRHS()));
        }
    });

    // II) compute unifying substitution
    Substitution substitution;

    // a utility for processing newly identified mappings
    auto newMapping = [&](const std::string& var, const AstArgument* term) {
        // found a new substitution
        Substitution newMapping(var, term);

        // apply substitution to all remaining equations
        for (auto& cur : equations) {
            cur.apply(newMapping);
        }

        // add mapping v -> t to substitution
        substitution.append(newMapping);
    };

    while (!equations.empty()) {
        // get next equation to compute
        Equation cur = equations.back();
        equations.pop_back();

        // shortcuts for left/right
        const AstArgument& a = *cur.lhs;
        const AstArgument& b = *cur.rhs;

        // #1:   t = t   => skip
        if (a == b) {
            continue;
        }

        // #2:   [..] = [..]   => decompose
        if (isRec(a) && isRec(b)) {
            // get arguments
            const auto& args_a = static_cast<const AstRecordInit&>(a).getArguments();
            const auto& args_b = static_cast<const AstRecordInit&>(b).getArguments();

            // make sure sizes are identical
            assert(args_a.size() == args_b.size());

            // create new equalities
            for (size_t i = 0; i < args_a.size(); ++i) {
                equations.push_back(Equation(args_a[i], args_b[i]));
            }
            continue;
        }

        // neither is a variable
        if (!isVar(a) && !isVar(b)) {
            continue;  // => nothing to do
        }

        // both are variables
        if (isVar(a) && isVar(b)) {
            // a new mapping is found
            auto& var = static_cast<const AstVariable&>(a);
            newMapping(var.getName(), &b);
            continue;
        }

        // #3:   t = v   => swap
        if (!isVar(a)) {
            equations.push_back(Equation(b, a));
            continue;
        }

        // now we know a is a variable
        assert(isVar(a));

        // we have   v = t
        const AstVariable& v = static_cast<const AstVariable&>(a);
        const AstArgument& t = b;

        // #4:   v occurs in t
        if (occurs(v, t)) {
            continue;
        }

        assert(!occurs(v, t));

        // add new maplet
        newMapping(v.getName(), &t);
    }

    // III) compute resulting clause
    return substitution(std::unique_ptr<AstClause>(clause.clone()));
}

std::unique_ptr<AstClause> ResolveAliasesTransformer::removeTrivialEquality(const AstClause& clause) {
    // finally: remove t = t constraints
    std::unique_ptr<AstClause> res(clause.cloneHead());
    for (AstLiteral* cur : clause.getBodyLiterals()) {
        // filter out t = t
        if (AstConstraint* rel = dynamic_cast<AstConstraint*>(cur)) {
            if (rel->getOperator() == BinaryConstraintOp::EQ) {
                if (*rel->getLHS() == *rel->getRHS()) {
                    continue;  // skip this one
                }
            }
        }
        res->addToBody(std::unique_ptr<AstLiteral>(cur->clone()));
    }

    // done
    return res;
}

void ResolveAliasesTransformer::removeComplexTermsInAtoms(AstClause& clause) {
    // restore temporary variables for expressions in atoms

    // get list of atoms
    std::vector<AstAtom*> atoms;
    for (AstLiteral* cur : clause.getBodyLiterals()) {
        if (AstAtom* arg = dynamic_cast<AstAtom*>(cur)) {
            atoms.push_back(arg);
        }
    }

    // find all binary operations in atoms
    std::vector<const AstArgument*> terms;
    for (const AstAtom* cur : atoms) {
        for (const AstArgument* arg : cur->getArguments()) {
            // only interested in functions
            if (!(dynamic_cast<const AstFunctor*>(arg))) {
                continue;
            }
            // add this one if not yet registered
            if (!any_of(terms, [&](const AstArgument* cur) { return *cur == *arg; })) {
                terms.push_back(arg);
            }
        }
    }

    // substitute them with variables (a real map would compare pointers)
    typedef std::vector<std::pair<std::unique_ptr<AstArgument>, std::unique_ptr<AstVariable>>>
            substitution_map;
    substitution_map map;

    int var_counter = 0;
    for (const AstArgument* arg : terms) {
        map.push_back(std::make_pair(std::unique_ptr<AstArgument>(arg->clone()),
                std::unique_ptr<AstVariable>(new AstVariable(" _tmp_" + toString(var_counter++)))));
    }

    // apply mapping to replace terms with variables
    struct Update : public AstNodeMapper {
        const substitution_map& map;
        Update(const substitution_map& map) : map(map) {}
        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            // check whether node needs to be replaced
            for (const auto& cur : map) {
                if (*cur.first == *node) {
                    return std::unique_ptr<AstNode>(cur.second->clone());
                }
            }
            // continue recursively
            node->apply(*this);
            return node;
        }
    };

    Update update(map);

    // update atoms
    for (AstAtom* atom : atoms) {
        atom->apply(update);
    }

    // add variable constraints to clause
    for (const auto& cur : map) {
        clause.addToBody(std::unique_ptr<AstLiteral>(
                new AstConstraint(BinaryConstraintOp::EQ, std::unique_ptr<AstArgument>(cur.second->clone()),
                        std::unique_ptr<AstArgument>(cur.first->clone()))));
    }
}

bool RemoveRelationCopiesTransformer::removeRelationCopies(AstProgram& program) {
    typedef std::map<AstRelationIdentifier, AstRelationIdentifier> alias_map;

    // tests whether something is a variable
    auto isVar = [&](const AstArgument& arg) { return dynamic_cast<const AstVariable*>(&arg); };

    // tests whether something is a record
    auto isRec = [&](const AstArgument& arg) { return dynamic_cast<const AstRecordInit*>(&arg); };

    // collect aliases
    alias_map isDirectAliasOf;

    // search for relations only defined by a single rule ..
    for (AstRelation* rel : program.getRelations()) {
        if (!rel->isComputed() && rel->getClauses().size() == 1u) {
            // .. of shape r(x,y,..) :- s(x,y,..)
            AstClause* cl = rel->getClause(0);
            if (!cl->isFact() && cl->getBodySize() == 1u && cl->getAtoms().size() == 1u) {
                AstAtom* atom = cl->getAtoms()[0];
                if (equal_targets(cl->getHead()->getArguments(), atom->getArguments())) {
                    // we have a match but have to check that all arguments are either
                    // variables or records containing variables
                    bool onlyVars = true;
                    auto args = cl->getHead()->getArguments();
                    while (!args.empty()) {
                        const auto& cur = args.back();
                        args.pop_back();
                        if (!isVar(*cur)) {
                            if (isRec(*cur)) {
                                // records are decomposed and their arguments are checked
                                const auto& rec_args = static_cast<const AstRecordInit&>(*cur).getArguments();
                                for (size_t i = 0; i < rec_args.size(); ++i) {
                                    args.push_back(rec_args[i]);
                                }
                            } else {
                                onlyVars = false;
                                break;
                            }
                        }
                    }
                    if (onlyVars) {
                        // all arguments are either variables or records containing variables
                        isDirectAliasOf[cl->getHead()->getName()] = atom->getName();
                    }
                }
            }
        }
    }

    // map each relation to its ultimate alias (could be transitive)
    alias_map isAliasOf;

    // track any copy cycles; cyclic rules are effectively empty
    std::set<AstRelationIdentifier> cycle_reps;

    for (std::pair<AstRelationIdentifier, AstRelationIdentifier> cur : isDirectAliasOf) {
        // compute replacement

        std::set<AstRelationIdentifier> visited;
        visited.insert(cur.first);
        visited.insert(cur.second);

        auto pos = isDirectAliasOf.find(cur.second);
        while (pos != isDirectAliasOf.end()) {
            if (visited.count(pos->second)) {
                cycle_reps.insert(cur.second);
                break;
            }
            cur.second = pos->second;
            pos = isDirectAliasOf.find(cur.second);
        }
        isAliasOf[cur.first] = cur.second;
    }

    if (isAliasOf.empty()) {
        return false;
    }

    // replace usage of relations according to alias map
    visitDepthFirst(program, [&](const AstAtom& atom) {
        auto pos = isAliasOf.find(atom.getName());
        if (pos != isAliasOf.end()) {
            const_cast<AstAtom&>(atom).setName(pos->second);
        }
    });

    // break remaining cycles
    for (const auto& rep : cycle_reps) {
        auto rel = program.getRelation(rep);
        rel->removeClause(rel->getClause(0));
    }

    // remove unused relations
    for (const auto& cur : isAliasOf) {
        if (!cycle_reps.count(cur.first)) {
            program.removeRelation(program.getRelation(cur.first)->getName());
        }
    }

    return true;
}

bool UniqueAggregationVariablesTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    // make variables in aggregates unique
    int aggNumber = 0;
    visitDepthFirstPostOrder(*translationUnit.getProgram(), [&](const AstAggregator& agg) {

        // only applicable for aggregates with target expression
        if (!agg.getTargetExpression()) {
            return;
        }

        // get all variables in the target expression
        std::set<std::string> names;
        visitDepthFirst(
                *agg.getTargetExpression(), [&](const AstVariable& var) { names.insert(var.getName()); });

        // rename them
        visitDepthFirst(agg, [&](const AstVariable& var) {
            auto pos = names.find(var.getName());
            if (pos == names.end()) {
                return;
            }
            const_cast<AstVariable&>(var).setName(" " + var.getName() + toString(aggNumber));
            changed = true;
        });

        // increment aggregation number
        aggNumber++;
    });
    return changed;
}

bool MaterializeAggregationQueriesTransformer::materializeAggregationQueries(
        AstTranslationUnit& translationUnit) {
    bool changed = false;

    AstProgram& program = *translationUnit.getProgram();
    const TypeEnvironment& env = translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();

    // if an aggregator has a body consisting of more than an atom => create new relation
    int counter = 0;
    visitDepthFirst(program, [&](const AstClause& clause) {
        visitDepthFirstPostOrder(clause, [&](const AstAggregator& agg) {
            // check whether a materialization is required
            if (!needsMaterializedRelation(agg)) {
                return;
            }

            changed = true;

            // for more body literals: create a new relation
            std::set<std::string> vars;
            visitDepthFirst(agg, [&](const AstVariable& var) { vars.insert(var.getName()); });

            // -- create a new clause --

            auto relName = "__agg_rel_" + toString(counter++);
            while (program.getRelation(relName) != nullptr) {
                relName = "__agg_rel_" + toString(counter++);
            }

            AstAtom* head = new AstAtom();
            head->setName(relName);
            std::vector<bool> symbolArguments;
            for (const auto& cur : vars) {
                head->addArgument(std::unique_ptr<AstArgument>(new AstVariable(cur)));
            }

            AstClause* aggClause = new AstClause();
            aggClause->setHead(std::unique_ptr<AstAtom>(head));
            for (const auto& cur : agg.getBodyLiterals()) {
                aggClause->addToBody(std::unique_ptr<AstLiteral>(cur->clone()));
            }

            // instantiate unnamed variables in count operations
            if (agg.getOperator() == AstAggregator::count) {
                int count = 0;
                for (const auto& cur : aggClause->getBodyLiterals()) {
                    cur->apply(
                            makeLambdaMapper([&](std::unique_ptr<AstNode> node) -> std::unique_ptr<AstNode> {
                                // check whether it is a unnamed variable
                                AstUnnamedVariable* var = dynamic_cast<AstUnnamedVariable*>(node.get());
                                if (!var) {
                                    return node;
                                }

                                // replace by variable
                                auto name = " _" + toString(count++);
                                auto res = new AstVariable(name);

                                // extend head
                                head->addArgument(std::unique_ptr<AstArgument>(res->clone()));

                                // return replacement
                                return std::unique_ptr<AstNode>(res);
                            }));
                }
            }

            // -- build relation --

            AstRelation* rel = new AstRelation();
            rel->setName(relName);
            // add attributes
            std::map<const AstArgument*, TypeSet> argTypes =
                    TypeAnalysis::analyseTypes(env, *aggClause, &program);
            for (const auto& cur : head->getArguments()) {
                rel->addAttribute(std::unique_ptr<AstAttribute>(new AstAttribute(
                        toString(*cur), (isNumberType(argTypes[cur])) ? "number" : "symbol")));
            }

            rel->addClause(std::unique_ptr<AstClause>(aggClause));
            program.appendRelation(std::unique_ptr<AstRelation>(rel));

            // -- update aggregate --
            AstAtom* aggAtom = head->clone();

            // count the usage of variables in the clause
            // outside of aggregates. Note that the visitor
            // is exhaustive hence double counting occurs
            // which needs to be deducted for variables inside
            // the aggregators and variables in the expression
            // of aggregate need to be added. Counter is zero
            // if the variable is local to the aggregate
            std::map<std::string, int> varCtr;
            visitDepthFirst(clause, [&](const AstArgument& arg) {
                if (const AstAggregator* a = dynamic_cast<const AstAggregator*>(&arg)) {
                    visitDepthFirst(arg, [&](const AstVariable& var) { varCtr[var.getName()]--; });
                    if (a->getTargetExpression() != nullptr) {
                        visitDepthFirst(*a->getTargetExpression(),
                                [&](const AstVariable& var) { varCtr[var.getName()]++; });
                    }
                } else {
                    visitDepthFirst(arg, [&](const AstVariable& var) { varCtr[var.getName()]++; });
                }
            });
            for (size_t i = 0; i < aggAtom->getArity(); i++) {
                if (AstVariable* var = dynamic_cast<AstVariable*>(aggAtom->getArgument(i))) {
                    // replace local variable by underscore if local
                    if (varCtr[var->getName()] == 0) {
                        aggAtom->setArgument(i, std::unique_ptr<AstArgument>(new AstUnnamedVariable()));
                    }
                }
            }
            const_cast<AstAggregator&>(agg).clearBodyLiterals();
            const_cast<AstAggregator&>(agg).addBodyLiteral(std::unique_ptr<AstLiteral>(aggAtom));
        });
    });

    return changed;
}

bool MaterializeAggregationQueriesTransformer::needsMaterializedRelation(const AstAggregator& agg) {
    // everything with more than 1 body literal => materialize
    if (agg.getBodyLiterals().size() > 1) {
        return true;
    }

    // inspect remaining atom more closely
    const AstAtom* atom = dynamic_cast<const AstAtom*>(agg.getBodyLiterals()[0]);
    assert(atom && "Body of aggregate is not containing an atom!");

    // if the same variable occurs several times => materialize
    bool duplicates = false;
    std::set<std::string> vars;
    visitDepthFirst(*atom,
            [&](const AstVariable& var) { duplicates = duplicates | !vars.insert(var.getName()).second; });

    // if there are duplicates a materialization is required
    if (duplicates) {
        return true;
    }

    // for all others the materialization can be skipped
    return false;
}

bool RemoveEmptyRelationsTransformer::removeEmptyRelations(AstTranslationUnit& translationUnit) {
    AstProgram& program = *translationUnit.getProgram();
    bool changed = false;
    for (auto rel : program.getRelations()) {
        if (rel->clauseSize() == 0 && !rel->isInput()) {
            removeEmptyRelationUses(translationUnit, rel);

            bool usedInAggregate = false;
            visitDepthFirst(program, [&](const AstAggregator& agg) {
                for (const auto lit : agg.getBodyLiterals()) {
                    visitDepthFirst(*lit, [&](const AstAtom& atom) {
                        if (getAtomRelation(&atom, &program) == rel) {
                            usedInAggregate = true;
                        }
                    });
                }
            });

            if (!usedInAggregate && !rel->isComputed()) {
                program.removeRelation(rel->getName());
            }
            changed = true;
        }
    }
    return changed;
}

void RemoveEmptyRelationsTransformer::removeEmptyRelationUses(
        AstTranslationUnit& translationUnit, AstRelation* emptyRelation) {
    AstProgram& program = *translationUnit.getProgram();

    //
    // (1) drop rules from the program that have empty relations in their bodies.
    // (2) drop negations of empty relations
    //
    // get all clauses
    std::vector<const AstClause*> clauses;
    visitDepthFirst(program, [&](const AstClause& cur) { clauses.push_back(&cur); });

    // clean all clauses
    for (const AstClause* cl : clauses) {
        // check for an atom whose relation is the empty relation

        bool removed = false;
        ;
        for (AstLiteral* lit : cl->getBodyLiterals()) {
            if (AstAtom* arg = dynamic_cast<AstAtom*>(lit)) {
                if (getAtomRelation(arg, &program) == emptyRelation) {
                    program.removeClause(cl);
                    removed = true;
                    break;
                }
            }
        }

        if (!removed) {
            // check whether a negation with empty relations exists

            bool rewrite = false;
            for (AstLiteral* lit : cl->getBodyLiterals()) {
                if (AstNegation* neg = dynamic_cast<AstNegation*>(lit)) {
                    if (getAtomRelation(neg->getAtom(), &program) == emptyRelation) {
                        rewrite = true;
                        break;
                    }
                }
            }

            if (rewrite) {
                // clone clause without negation for empty relations

                auto res = std::unique_ptr<AstClause>(cl->cloneHead());

                for (AstLiteral* lit : cl->getBodyLiterals()) {
                    if (AstNegation* neg = dynamic_cast<AstNegation*>(lit)) {
                        if (getAtomRelation(neg->getAtom(), &program) != emptyRelation) {
                            res->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
                        }
                    } else {
                        res->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
                    }
                }

                program.removeClause(cl);
                program.appendClause(std::move(res));
            }
        }
    }
}

bool RemoveRedundantRelationsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;
    RedundantRelations* redundantRelationsAnalysis = translationUnit.getAnalysis<RedundantRelations>();
    const std::set<const AstRelation*>& redundantRelations =
            redundantRelationsAnalysis->getRedundantRelations();
    if (!redundantRelations.empty()) {
        for (auto rel : redundantRelations) {
            translationUnit.getProgram()->removeRelation(rel->getName());
            changed = true;
        }
    }
    return changed;
}

bool NormaliseConstraintsTransformer::transform(AstTranslationUnit& translationUnit) {
    bool changed = false;

    // set a prefix for variables bound by magic-set for identification later
    // prepended by + to avoid conflict with user-defined variables
    const std::string boundPrefix = "+abdul";

    AstProgram* program = translationUnit.getProgram();
    std::vector<AstRelation*> relations = program->getRelations();

    int constantCount = 0;    // number of constants seen so far
    int underscoreCount = 0;  // number of underscores seen so far

    // go through the relations, replacing constants and underscores with constraints and variables
    for (AstRelation* rel : relations) {
        std::vector<AstClause*> clauses = rel->getClauses();
        for (AstClause* clause : clauses) {
            // create the replacement clause
            AstClause* newClause = clause->cloneHead();

            for (AstLiteral* lit : clause->getBodyLiterals()) {
                // if not an atom, just add it immediately
                if (dynamic_cast<AstAtom*>(lit) == 0) {
                    newClause->addToBody(std::unique_ptr<AstLiteral>(lit->clone()));
                    continue;
                }

                AstAtom* newAtom = lit->getAtom()->clone();
                std::vector<AstArgument*> args = newAtom->getArguments();
                for (size_t argNum = 0; argNum < args.size(); argNum++) {
                    AstArgument* currArg = args[argNum];

                    // check if the argument is constant
                    if (dynamic_cast<const AstConstant*>(currArg)) {
                        // constant found
                        changed = true;

                        // create new variable name (with appropriate suffix)
                        std::stringstream newVariableName;
                        std::stringstream currArgName;
                        currArgName << *currArg;

                        if (dynamic_cast<AstNumberConstant*>(currArg)) {
                            newVariableName << boundPrefix << constantCount << "_" << currArgName.str()
                                            << "_n";
                        } else {
                            newVariableName << boundPrefix << constantCount << "_"
                                            << currArgName.str().substr(1, currArgName.str().size() - 2)
                                            << "_s";
                        }

                        AstArgument* variable = new AstVariable(newVariableName.str());
                        AstArgument* constant = currArg->clone();

                        constantCount++;

                        // update argument to be the variable created
                        newAtom->setArgument(argNum, std::unique_ptr<AstArgument>(variable));

                        // add in constraint (+abdulX = constant)
                        newClause->addToBody(std::unique_ptr<AstLiteral>(new AstConstraint(
                                BinaryConstraintOp::EQ, std::unique_ptr<AstArgument>(variable->clone()),
                                std::unique_ptr<AstArgument>(constant->clone()))));
                    } else if (dynamic_cast<const AstUnnamedVariable*>(currArg)) {
                        // underscore found
                        changed = true;

                        // create new variable name for the underscore (with appropriate suffix)
                        std::stringstream newVariableName;
                        newVariableName.str("");
                        newVariableName << "+underscore" << underscoreCount;
                        underscoreCount++;

                        AstArgument* variable = new AstVariable(newVariableName.str());

                        // update argument to be a variable
                        newAtom->setArgument(argNum, std::unique_ptr<AstArgument>(variable));
                    }
                }

                // add the new atom to the body
                newClause->addToBody(std::unique_ptr<AstLiteral>(newAtom));
            }

            // swap out the old clause with the new one
            rel->removeClause(clause);
            rel->addClause(std::unique_ptr<AstClause>(newClause));
        }
    }

    return changed;
}

}  // end of namespace souffle
