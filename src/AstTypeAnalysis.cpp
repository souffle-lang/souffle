/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTypeAnalysis.cpp
 *
 * Implements an AST Constraint Analysis Infrastructure and contains
 * a collection of type analyses operating on AST constructs.
 *
 ***********************************************************************/

#include "AstTypeAnalysis.h"
#include "AstArgument.h"
#include "AstAttribute.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"
#include "AstType.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "BinaryConstraintOps.h"
#include "Constraints.h"
#include "Global.h"
#include "Util.h"
#include <cstddef>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <utility>

namespace souffle {

namespace {

/**
 * A variable type to be utilized by AST constraint analysis. Each such variable is
 * associated with an AstArgument which's property it is describing.
 *
 * @tparam PropertySpace the property space associated to the analysis
 */
template <typename PropertySpace>
struct AstConstraintAnalysisVar : public Variable<const AstArgument*, PropertySpace> {
    explicit AstConstraintAnalysisVar(const AstArgument* arg)
            : Variable<const AstArgument*, PropertySpace>(arg) {}
    explicit AstConstraintAnalysisVar(const AstArgument& arg)
            : Variable<const AstArgument*, PropertySpace>(&arg) {}

    /** adds print support */
    void print(std::ostream& out) const override {
        out << "var(" << *(this->id) << ")";
    }
};

/**
 * A base class for AstConstraintAnalysis collecting constraints for an analysis
 * by visiting every node of a given AST. The collected constraints are
 * then utilized to obtain the desired analysis result.
 *
 * @tparam AnalysisVar the type of variable (and included property space)
 *      to be utilized by this analysis.
 */
template <typename AnalysisVar>
class AstConstraintAnalysis : public AstVisitor<void> {
    using value_type = typename AnalysisVar::property_space::value_type;

    /** The list of constraints making underlying this analysis */
    Problem<AnalysisVar> constraints;

    /** A map mapping variables to unique instances to facilitate the unification of variables */
    std::map<std::string, AnalysisVar> variables;

protected:
    // a few type definitions
    using constraint_type = std::shared_ptr<Constraint<AnalysisVar>>;
    using solution_type = std::map<const AstArgument*, value_type>;

    /**
     * A utility function mapping an AstArgument to its associated analysis variable.
     *
     * @param arg the AST argument to be mapped
     * @return the analysis variable representing its associated value
     */
    AnalysisVar getVar(const AstArgument& arg) {
        const auto* var = dynamic_cast<const AstVariable*>(&arg);
        if (!var) {
            // no mapping required
            return AnalysisVar(arg);
        }

        // filter through map => always take the same variable
        auto res = variables.insert(std::make_pair(var->getName(), AnalysisVar(var)));
        return res.first->second;
    }

    /**
     * A utility function mapping an AstArgument to its associated analysis variable.
     *
     * @param arg the AST argument to be mapped
     * @return the analysis variable representing its associated value
     */
    AnalysisVar getVar(const AstArgument* arg) {
        return getVar(*arg);
    }

    /** Adds another constraint to the internally maintained list of constraints */
    void addConstraint(const constraint_type& constraint) {
        constraints.add(constraint);
    }

public:
    /**
     * Runs this constraint analysis on the given clause.
     *
     * @param clause the close to be analysed
     * @param debug a flag enabling the printing of debug information
     * @return an assignment mapping a property to each argument in the given clause
     */
    solution_type analyse(const AstClause& clause, std::ostream* debugOutput = nullptr) {
        // collect constraints
        visitDepthFirstPreOrder(clause, *this);

        // solve constraints
        auto ass = constraints.solve();

        // print debug information if desired
        if (debugOutput != nullptr) {
            *debugOutput << "Clause: " << clause << "\n";
            *debugOutput << "Problem:\n" << constraints << "\n";
            *debugOutput << "Solution:\n" << ass << "\n";
        }

        // convert assignment to result
        solution_type res;
        visitDepthFirst(clause, [&](const AstArgument& cur) { res[&cur] = ass[getVar(cur)]; });
        return res;
    }
};

}  // end namespace

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

std::map<const AstArgument*, bool> getGroundedTerms(const AstClause& clause) {
    struct Analysis : public AstConstraintAnalysis<BoolDisjunctVar> {
        std::set<const AstAtom*> ignore;

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

        // also skip head
        void visitClause(const AstClause& clause) override {
            // ignore head
            ignore.insert(clause.getHead());
        }

        // binary equality relations propagates groundness
        void visitBinaryConstraint(const AstBinaryConstraint& cur) override {
            // only target equality
            if (cur.getOperator() != BinaryConstraintOp::EQ) {
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

        // unary functors with grounded values are grounded values
        void visitUnaryFunctor(const AstUnaryFunctor& cur) override {
            auto fun = getVar(cur);
            auto arg = getVar(cur.getOperand());

            addConstraint(imply(arg, fun));
        }

        // binary functors with grounded values are grounded values
        void visitBinaryFunctor(const AstBinaryFunctor& cur) override {
            auto fun = getVar(cur);
            auto lhs = getVar(cur.getLHS());
            auto rhs = getVar(cur.getRHS());

            addConstraint(imply({lhs, rhs}, fun));
        }

        // ternary functors with grounded values are grounded values
        void visitTernaryFunctor(const AstTernaryFunctor& cur) override {
            auto fun = getVar(cur);
            auto a0 = getVar(cur.getArg(0));
            auto a1 = getVar(cur.getArg(1));
            auto a2 = getVar(cur.getArg(2));

            addConstraint(imply({a0, a1, a2}, fun));
        }

        // user-defined functors with grounded values are grounded values
        void visitUserDefinedFunctor(const AstUserDefinedFunctor& cur) override {
            auto fun = getVar(cur);
            std::vector<BoolDisjunctVar> varArgs;
            for (const auto& arg : cur.getArguments()) {
                varArgs.push_back(getVar(arg));
            }
            addConstraint(imply(varArgs, fun));
        }
    };

    // run analysis on given clause
    return Analysis().analyse(clause);
}

namespace {

// -----------------------------------------------------------------------------
//                          Type Deduction Lattice
// -----------------------------------------------------------------------------

/**
 * An implementation of a meet operation between sets of types computing
 * the set of pair-wise greatest common subtypes.
 */
struct sub_type {
    bool operator()(TypeSet& a, const TypeSet& b) const {
        // compute result set
        TypeSet res = getGreatestCommonSubtypes(a, b);

        // check whether a should change
        if (res == a) {
            return false;
        }

        // update a
        a = res;
        return true;
    }
};

/**
 * A factory for computing sets of types covering all potential types.
 */
struct all_type_factory {
    TypeSet operator()() const {
        return TypeSet::getAllTypes();
    }
};

/**
 * The type lattice forming the property space for the Type analysis. The
 * value set is given by sets of types and the meet operator is based on the
 * pair-wise computation of greatest common subtypes. Correspondingly, the
 * bottom element has to be the set of all types.
 */
struct type_lattice : public property_space<TypeSet, sub_type, all_type_factory> {};

/** The definition of the type of variable to be utilized in the type analysis */
using TypeVar = AstConstraintAnalysisVar<type_lattice>;

/** The definition of the type of constraint to be utilized in the type analysis */
using TypeConstraint = std::shared_ptr<Constraint<TypeVar>>;

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of the variable b.
 */
TypeConstraint isSubtypeOf(const TypeVar& a, const TypeVar& b) {
    return sub(a, b, "<:");
}

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of type b.
 */
TypeConstraint isSubtypeOf(const TypeVar& a, const Type& b) {
    struct C : public Constraint<TypeVar> {
        TypeVar a;
        const Type& b;

        C(TypeVar a, const Type& b) : a(std::move(a)), b(b) {}

        bool update(Assignment<TypeVar>& ass) const override {
            // get current value of variable a
            TypeSet& s = ass[a];

            // remove all types that are not sub-types of b
            if (s.isAll()) {
                s = TypeSet(b);
                return true;
            }

            TypeSet res;
            for (const Type& t : s) {
                res.insert(getGreatestCommonSubtypes(t, b));
            }

            // check whether there was a change
            if (res == s) {
                return false;
            }
            s = res;
            return true;
        }

        void print(std::ostream& out) const override {
            out << a << " <: " << b.getName();
        }
    };

    return std::make_shared<C>(a, b);
}

/**
 * A constraint factory ensuring that all the types associated to the variable
 * a are subtypes of type b.
 */
TypeConstraint isSupertypeOf(const TypeVar& a, const Type& b) {
    struct C : public Constraint<TypeVar> {
        TypeVar a;
        const Type& b;
        mutable bool repeat;

        C(TypeVar a, const Type& b) : a(std::move(a)), b(b), repeat(true) {}

        bool update(Assignment<TypeVar>& ass) const override {
            // don't continually update super type constraints
            if (!repeat) {
                return false;
            }
            repeat = false;

            // get current value of variable a
            TypeSet& s = ass[a];

            // remove all types that are not super-types of b
            if (s.isAll()) {
                s = TypeSet(b);
                return true;
            }

            TypeSet res;
            for (const Type& t : s) {
                res.insert(getLeastCommonSupertypes(t, b));
            }

            // check whether there was a change
            if (res == s) {
                return false;
            }
            s = res;
            return true;
        }

        void print(std::ostream& out) const override {
            out << a << " >: " << b.getName();
        }
    };

    return std::make_shared<C>(a, b);
}

TypeConstraint isSubtypeOfComponent(const TypeVar& a, const TypeVar& b, int index) {
    struct C : public Constraint<TypeVar> {
        TypeVar a;
        TypeVar b;
        unsigned index;

        C(TypeVar a, TypeVar b, int index) : a(std::move(a)), b(std::move(b)), index(index) {}

        bool update(Assignment<TypeVar>& ass) const override {
            // get list of types for b
            const TypeSet& recs = ass[b];

            // if it is (not yet) constrainted => skip
            if (recs.isAll()) {
                return false;
            }

            // compute new types for a and b
            TypeSet typesA;
            TypeSet typesB;

            // iterate through types of b
            for (const Type& t : recs) {
                // only retain records
                if (!isRecordType(t)) {
                    continue;
                }
                const auto& rec = static_cast<const RecordType&>(t);

                // of proper size
                if (rec.getFields().size() <= index) {
                    continue;
                }

                // this is a valid type for b
                typesB.insert(rec);

                // and its corresponding field for a
                typesA.insert(rec.getFields()[index].type);
            }

            // combine with current types assigned to a
            typesA = getGreatestCommonSubtypes(ass[a], typesA);

            // update values
            bool changed = false;
            if (recs != typesB) {
                ass[b] = typesB;
                changed = true;
            }

            if (ass[a] != typesA) {
                ass[a] = typesA;
                changed = true;
            }

            // done
            return changed;
        }

        void print(std::ostream& out) const override {
            out << a << " <: " << b << "::" << index;
        }
    };

    return std::make_shared<C>(a, b, index);
}
}  // namespace

void TypeEnvironmentAnalysis::run(const AstTranslationUnit& translationUnit) {
    updateTypeEnvironment(*translationUnit.getProgram());
}

void TypeEnvironmentAnalysis::print(std::ostream& os) const {
    env.print(os);
}

/**
 * A utility function utilized by the finishParsing member function to update a type environment
 * out of a given list of types in the AST
 *
 * @param types the types specified in the input file, contained in the AST
 * @param env the type environment to be updated
 */
void TypeEnvironmentAnalysis::updateTypeEnvironment(const AstProgram& program) {
    // build up new type system based on defined types

    // create all type symbols in a first step
    for (const auto& cur : program.getTypes()) {
        // support faulty codes with multiple definitions
        if (env.isType(cur->getName())) {
            continue;
        }

        // create type within type environment
        if (auto* t = dynamic_cast<const AstPrimitiveType*>(cur)) {
            if (t->isNumeric()) {
                env.createNumericType(cur->getName());
            } else {
                env.createSymbolType(cur->getName());
            }
        } else if (dynamic_cast<const AstUnionType*>(cur)) {
            // initialize the union
            env.createUnionType(cur->getName());
        } else if (dynamic_cast<const AstRecordType*>(cur)) {
            // initialize the record
            env.createRecordType(cur->getName());
        } else {
            std::cout << "Unsupported type construct: " << typeid(cur).name() << "\n";
            assert(false && "Unsupported Type Construct!");
        }
    }

    // link symbols in a second step
    for (const auto& cur : program.getTypes()) {
        Type* type = env.getModifiableType(cur->getName());
        assert(type && "It should be there!");

        if (dynamic_cast<const AstPrimitiveType*>(cur)) {
            // nothing to do here
        } else if (auto* t = dynamic_cast<const AstUnionType*>(cur)) {
            // get type as union type
            auto* ut = dynamic_cast<UnionType*>(type);
            if (!ut) {
                continue;  // support faulty input
            }

            // add element types
            for (const auto& cur : t->getTypes()) {
                if (env.isType(cur)) {
                    ut->add(env.getType(cur));
                }
            }
        } else if (auto* t = dynamic_cast<const AstRecordType*>(cur)) {
            // get type as record type
            auto* rt = dynamic_cast<RecordType*>(type);
            if (!rt) {
                continue;  // support faulty input
            }

            // add fields
            for (const auto& f : t->getFields()) {
                if (env.isType(f.type)) {
                    rt->add(f.name, env.getType(f.type));
                }
            }
        } else {
            std::cout << "Unsupported type construct: " << typeid(cur).name() << "\n";
            assert(false && "Unsupported Type Construct!");
        }
    }
}

/* Return a new clause with type-annotated variables */
AstClause* createAnnotatedClause(
        const AstClause* clause, const std::map<const AstArgument*, TypeSet> argumentTypes) {
    // Annotates each variable with its type based on a given type analysis result
    struct TypeAnnotator : public AstNodeMapper {
        const std::map<const AstArgument*, TypeSet>& types;

        TypeAnnotator(const std::map<const AstArgument*, TypeSet>& types) : types(types) {}

        std::unique_ptr<AstNode> operator()(std::unique_ptr<AstNode> node) const override {
            if (auto* var = dynamic_cast<AstVariable*>(node.get())) {
                std::stringstream newVarName;
                newVarName << var->getName() << "&isin;" << types.find(var)->second;
                return std::make_unique<AstVariable>(newVarName.str());
            } else if (auto* var = dynamic_cast<AstUnnamedVariable*>(node.get())) {
                std::stringstream newVarName;
                newVarName << "_"
                           << "&isin;" << types.find(var)->second;
                return std::make_unique<AstVariable>(newVarName.str());
            }
            node->apply(*this);
            return node;
        }
    };

    /* Note:
     * Because the type of each argument is stored in the form [address -> type-set],
     * the type-analysis result does not immediately apply to the clone due to differing
     * addresses.
     * Two ways around this:
     *  (1) Perform the type-analysis again for the cloned clause
     *  (2) Keep track of the addresses of equivalent arguments in the cloned clause
     * Method (2) was chosen to avoid having to recompute the analysis each time.
     */
    AstClause* annotatedClause = clause->clone();

    // Maps x -> y, where x is the address of an argument in the original clause, and y
    // is the address of the equivalent argument in the clone.
    std::map<const AstArgument*, const AstArgument*> memoryMap;

    std::vector<const AstArgument*> originalAddresses;
    visitDepthFirst(*clause, [&](const AstArgument& arg) { originalAddresses.push_back(&arg); });

    std::vector<const AstArgument*> cloneAddresses;
    visitDepthFirst(*annotatedClause, [&](const AstArgument& arg) { cloneAddresses.push_back(&arg); });

    assert(cloneAddresses.size() == originalAddresses.size());

    for (size_t i = 0; i < originalAddresses.size(); i++) {
        memoryMap[originalAddresses[i]] = cloneAddresses[i];
    }

    // Map the types to the clause clone
    std::map<const AstArgument*, TypeSet> cloneArgumentTypes;
    for (auto& pair : argumentTypes) {
        cloneArgumentTypes[memoryMap[pair.first]] = pair.second;
    }

    // Create the type-annotated clause
    TypeAnnotator annotator(cloneArgumentTypes);
    annotatedClause->apply(annotator);
    return annotatedClause;
}

void TypeAnalysis::run(const AstTranslationUnit& translationUnit) {
    // Check if debugging information is being generated and note where logs should be sent
    std::ostream* debugStream = nullptr;
    if (!Global::config().get("debug-report").empty()) {
        debugStream = &analysisLogs;
    }
    auto* typeEnvAnalysis = translationUnit.getAnalysis<TypeEnvironmentAnalysis>();
    for (const AstRelation* rel : translationUnit.getProgram()->getRelations()) {
        for (const AstClause* clause : rel->getClauses()) {
            // Perform the type analysis
            std::map<const AstArgument*, TypeSet> clauseArgumentTypes =
                    analyseTypes(typeEnvAnalysis->getTypeEnvironment(), *clause, translationUnit.getProgram(),
                            debugStream);
            argumentTypes.insert(clauseArgumentTypes.begin(), clauseArgumentTypes.end());

            if (debugStream != nullptr) {
                // Store an annotated clause for printing purposes
                AstClause* annotatedClause = createAnnotatedClause(clause, clauseArgumentTypes);
                annotatedClauses.push_back(std::unique_ptr<AstClause>(annotatedClause));
            }
        }
    }
}

void TypeAnalysis::print(std::ostream& os) const {
    os << "-- Analysis logs --" << std::endl;
    os << analysisLogs.str() << std::endl;
    os << "-- Result --" << std::endl;
    for (const auto& curr : annotatedClauses) {
        os << *curr << std::endl;
    }
}

/**
 * Generic type analysis framework for clauses
 */

std::map<const AstArgument*, TypeSet> TypeAnalysis::analyseTypes(
        const TypeEnvironment& env, const AstClause& clause, const AstProgram* program, std::ostream* logs) {
    struct Analysis : public AstConstraintAnalysis<TypeVar> {
        const TypeEnvironment& env;
        const AstProgram* program;
        std::set<const AstAtom*> negated;

        Analysis(const TypeEnvironment& env, const AstProgram* program) : env(env), program(program) {}

        // predicate
        void visitAtom(const AstAtom& atom) override {
            // get relation
            auto rel = getAtomRelation(&atom, program);
            if (!rel) {
                return;  // error in input program
            }

            auto atts = rel->getAttributes();
            auto args = atom.getArguments();
            if (atts.size() != args.size()) {
                return;  // error in input program
            }

            // set upper boundary of argument types
            for (unsigned i = 0; i < atts.size(); i++) {
                const auto& typeName = atts[i]->getTypeName();
                if (env.isType(typeName)) {
                    // check whether atom is not negated
                    if (negated.find(&atom) == negated.end()) {
                        addConstraint(isSubtypeOf(getVar(args[i]), env.getType(typeName)));
                    } else {
                        addConstraint(isSupertypeOf(getVar(args[i]), env.getType(typeName)));
                    }
                }
            }
        }

        // negations need to be skipped
        void visitNegation(const AstNegation& cur) override {
            // add nested atom to black-list
            negated.insert(cur.getAtom());
        }

        // symbol
        void visitStringConstant(const AstStringConstant& cnst) override {
            // this type has to be a sub-type of symbol
            addConstraint(isSubtypeOf(getVar(cnst), env.getSymbolType()));
        }

        // number
        void visitNumberConstant(const AstNumberConstant& cnst) override {
            // this type has to be a sub-type of number
            addConstraint(isSubtypeOf(getVar(cnst), env.getNumberType()));
        }

        // binary constraint
        void visitBinaryConstraint(const AstBinaryConstraint& rel) override {
            auto lhs = getVar(rel.getLHS());
            auto rhs = getVar(rel.getRHS());
            addConstraint(isSubtypeOf(lhs, rhs));
            addConstraint(isSubtypeOf(rhs, lhs));
        }

        // unary functor
        void visitUnaryFunctor(const AstUnaryFunctor& fun) override {
            auto out = getVar(fun);
            auto in = getVar(fun.getOperand());

            // add a constraint for the return type of the unary functor
            if (fun.isNumerical()) {
                addConstraint(isSubtypeOf(out, env.getNumberType()));
            }
            if (fun.isSymbolic()) {
                addConstraint(isSubtypeOf(out, env.getSymbolType()));
            }

            // add a constraint for the argument type of the unary functor
            if (fun.acceptsNumbers()) {
                addConstraint(isSubtypeOf(in, env.getNumberType()));
            }
            if (fun.acceptsSymbols()) {
                addConstraint(isSubtypeOf(in, env.getSymbolType()));
            }
        }

        // binary functor
        void visitBinaryFunctor(const AstBinaryFunctor& fun) override {
            auto cur = getVar(fun);
            auto lhs = getVar(fun.getLHS());
            auto rhs = getVar(fun.getRHS());

            // add a constraint for the return type of the binary functor
            if (fun.isNumerical()) {
                addConstraint(isSubtypeOf(cur, env.getNumberType()));
            }
            if (fun.isSymbolic()) {
                addConstraint(isSubtypeOf(cur, env.getSymbolType()));
            }

            // add a constraint for the first argument of the binary functor
            if (fun.acceptsNumbers(0)) {
                addConstraint(isSubtypeOf(lhs, env.getNumberType()));
            }
            if (fun.acceptsSymbols(0)) {
                addConstraint(isSubtypeOf(lhs, env.getSymbolType()));
            }

            // add a constraint for the second argument of the binary functor
            if (fun.acceptsNumbers(1)) {
                addConstraint(isSubtypeOf(rhs, env.getNumberType()));
            }
            if (fun.acceptsSymbols(1)) {
                addConstraint(isSubtypeOf(rhs, env.getSymbolType()));
            }
        }

        // ternary functor
        void visitTernaryFunctor(const AstTernaryFunctor& fun) override {
            auto cur = getVar(fun);

            auto a0 = getVar(fun.getArg(0));
            auto a1 = getVar(fun.getArg(1));
            auto a2 = getVar(fun.getArg(2));

            // add a constraint for the return type of the ternary functor
            if (fun.isNumerical()) {
                addConstraint(isSubtypeOf(cur, env.getNumberType()));
            }
            if (fun.isSymbolic()) {
                addConstraint(isSubtypeOf(cur, env.getSymbolType()));
            }

            // add a constraint for the first argument of the ternary functor
            if (fun.acceptsNumbers(0)) {
                addConstraint(isSubtypeOf(a0, env.getNumberType()));
            }
            if (fun.acceptsSymbols(0)) {
                addConstraint(isSubtypeOf(a0, env.getSymbolType()));
            }

            // add a constraint for the second argument of the ternary functor
            if (fun.acceptsNumbers(1)) {
                addConstraint(isSubtypeOf(a1, env.getNumberType()));
            }
            if (fun.acceptsSymbols(1)) {
                addConstraint(isSubtypeOf(a1, env.getSymbolType()));
            }

            // add a constraint for the third argument of the ternary functor
            if (fun.acceptsNumbers(2)) {
                addConstraint(isSubtypeOf(a2, env.getNumberType()));
            }
            if (fun.acceptsSymbols(2)) {
                addConstraint(isSubtypeOf(a2, env.getSymbolType()));
            }
        }

        // user-defined functors
        void visitUserDefinedFunctor(const AstUserDefinedFunctor& fun) override {
            auto cur = getVar(fun);

            // get functor declaration
            const AstFunctorDeclaration* funDecl = program->getFunctorDeclaration(fun.getName());
            // check whether functor declaration exists
            if (funDecl != nullptr) {
                // add a constraint for the return type
                if (funDecl->isNumerical()) {
                    addConstraint(isSubtypeOf(cur, env.getNumberType()));
                }
                if (funDecl->isSymbolic()) {
                    addConstraint(isSubtypeOf(cur, env.getSymbolType()));
                }

                // add constraints for arguments
                for (size_t i = 0; i < fun.getArgCount(); i++) {
                    auto arg = getVar(fun.getArg(i));

                    // check that usage does not exceed
                    // number of arguments in declaration
                    if (i < funDecl->getArgCount()) {
                        // add constraints for the i-th argument
                        if (funDecl->acceptsNumbers(i)) {
                            addConstraint(isSubtypeOf(arg, env.getNumberType()));
                        }
                        if (funDecl->acceptsSymbols(i)) {
                            addConstraint(isSubtypeOf(arg, env.getSymbolType()));
                        }
                    }
                }
            }
        }

        // counter
        void visitCounter(const AstCounter& counter) override {
            // this value must be a number value
            addConstraint(isSubtypeOf(getVar(counter), env.getNumberType()));
        }

        // components of records
        void visitRecordInit(const AstRecordInit& init) override {
            // link element types with sub-values
            auto rec = getVar(init);
            int i = 0;

            for (const AstArgument* value : init.getArguments()) {
                addConstraint(isSubtypeOfComponent(getVar(value), rec, i++));
            }
        }

        // visit aggregates
        void visitAggregator(const AstAggregator& agg) override {
            // this value must be a number value
            addConstraint(isSubtypeOf(getVar(agg), env.getNumberType()));

            // also, the target expression needs to be a number
            if (auto expr = agg.getTargetExpression()) {
                addConstraint(isSubtypeOf(getVar(expr), env.getNumberType()));
            }
        }
    };

    // run analysis
    return Analysis(env, program).analyse(clause, logs);
}

}  // end of namespace souffle
