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
 * Implements a collection of type analyses operating on AST constructs.
 *
 ***********************************************************************/

#include "AstTypeAnalysis.h"
#include "AstArgument.h"
#include "AstAttribute.h"
#include "AstClause.h"
#include "AstConstraintAnalysis.h"
#include "AstLiteral.h"
#include "AstNode.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"
#include "AstType.h"
#include "AstTypeEnvironmentAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "Constraints.h"
#include "Global.h"
#include "TypeSystem.h"
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace souffle {

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
    for (const auto& cur : annotatedClauses) {
        os << *cur << std::endl;
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
