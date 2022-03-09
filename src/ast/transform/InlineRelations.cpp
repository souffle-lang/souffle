/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InlineRelations.cpp
 *
 * Define classes and functionality related to inlining.
 *
 ***********************************************************************/

#include "ast/transform/InlineRelations.h"
#include "AggregateOp.h"
#include "Global.h"
#include "RelationTag.h"
#include "ast/Aggregator.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/Constraint.h"
#include "ast/Functor.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/TypeCast.h"
#include "ast/UnnamedVariable.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "ast/analysis/typesystem/PolymorphicObjects.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast::transform {

using ExcludedRelations = InlineRelationsTransformer::ExcludedRelations;

template <class T>
class NullableVector {
private:
    std::vector<T> vector;
    bool valid = false;

public:
    NullableVector() = default;
    NullableVector(std::vector<T> vector) : vector(std::move(vector)), valid(true) {}

    bool isValid() const {
        return valid;
    }

    const std::vector<T>& getVector() const {
        assert(valid && "Accessing invalid vector!");
        return vector;
    }
};

NullableVector<std::vector<Literal*>> getInlinedLiteral(Program&, Literal*);

/**
 * Replace constants in the head of inlined clauses with (constrained) variables.
 */
bool normaliseInlinedHeads(Program& program) {
    bool changed = false;
    static int newVarCount = 0;

    // Go through the clauses of all inlined relations
    for (Relation* rel : program.getRelations()) {
        if (!rel->hasQualifier(RelationQualifier::INLINE)) {
            continue;
        }

        for (auto&& clause : program.getClauses(*rel)) {
            // Set up the new clause with an empty body and no arguments in the head
            auto newClause = mk<Clause>(clause->getHead()->getQualifiedName(), clause->getSrcLoc());
            newClause->setBodyLiterals(clone(clause->getBodyLiterals()));
            auto clauseHead = newClause->getHead();

            // Set up the head arguments in the new clause
            for (Argument* arg : clause->getHead()->getArguments()) {
                bool isConstrained = false;
                if (auto* var = as<Variable>(arg)) {
                    for (Argument* prevVar : clauseHead->getArguments()) {
                        // check whether same variable showing up in head again
                        if (*prevVar == *var) {
                            isConstrained = true;
                            break;
                        }
                    }
                } else {
                    // this is a complex term (constant, functor, etc.)
                    // and needs to be replaced.
                    isConstrained = true;
                }
                if (isConstrained) {
                    // Found a non-variable/reoccurring in the head, so replace it with a new variable
                    // and construct a new equivalence constraint with the argument term.
                    std::stringstream newVar;
                    newVar << "<new_var_" << newVarCount++ << ">";
                    clauseHead->addArgument(mk<ast::Variable>(newVar.str()));

                    // Add a body constraint to set the variable's value to be the original constant
                    newClause->addToBody(mk<BinaryConstraint>(
                            BinaryConstraintOp::EQ, mk<ast::Variable>(newVar.str()), clone(arg)));
                } else {
                    clauseHead->addArgument(clone(arg));
                }
            }

            // Replace the old clause with this one
            program.addClause(std::move(newClause));
            program.removeClause(*clause);
            changed = true;
        }
    }

    return changed;
}

/**
 * Removes all underscores in all atoms of inlined relations
 */
bool nameInlinedUnderscores(Program& program) {
    struct M : public NodeMapper {
        mutable bool changed = false;
        const std::set<QualifiedName> inlinedRelations;
        bool replaceUnderscores;

        M(std::set<QualifiedName> inlinedRelations, bool replaceUnderscores)
                : inlinedRelations(std::move(inlinedRelations)), replaceUnderscores(replaceUnderscores) {}

        Own<Node> operator()(Own<Node> node) const override {
            static int underscoreCount = 0;

            if (!replaceUnderscores) {
                // Check if we should start replacing underscores for this node's subnodes
                if (auto* atom = as<Atom>(node)) {
                    if (inlinedRelations.find(atom->getQualifiedName()) != inlinedRelations.end()) {
                        // Atom associated with an inlined relation, so replace the underscores
                        // in all of its subnodes with named variables.
                        M replace(inlinedRelations, true);
                        node->apply(replace);
                        changed |= replace.changed;
                        return node;
                    }
                }
            } else if (isA<UnnamedVariable>(node)) {
                // Give a unique name to the underscored variable
                // TODO (azreika): need a more consistent way of handling internally generated variables in
                // general
                std::stringstream newVarName;
                newVarName << "<underscore_" << underscoreCount++ << ">";
                changed = true;
                return mk<ast::Variable>(newVarName.str());
            }

            node->apply(*this);
            return node;
        }
    };

    // Store the names of all relations to be inlined
    std::set<QualifiedName> inlinedRelations;
    for (Relation* rel : program.getRelations()) {
        if (rel->hasQualifier(RelationQualifier::INLINE)) {
            inlinedRelations.insert(rel->getQualifiedName());
        }
    }

    // Apply the renaming procedure to the entire program
    M update(inlinedRelations, false);
    program.apply(update);
    return update.changed;
}

/**
 * Checks if a given clause contains an atom that should be inlined.
 */
bool containsInlinedAtom(const Program& program, const Clause& clause) {
    auto hasInlinedAtom = [&](const Atom& atom) {
        Relation* rel = program.getRelation(atom);
        return rel->hasQualifier(RelationQualifier::INLINE);
    };
    return visitExists(clause.getHead()->getArguments(), hasInlinedAtom) ||
           visitExists(clause.getBodyLiterals(), hasInlinedAtom);
}

/**
 * Reduces a vector of substitutions.
 * Returns false only if matched argument pairs are found to be incompatible.
 */
bool reduceSubstitution(std::vector<std::pair<Argument*, Argument*>>& sub) {
    // Keep trying to reduce the substitutions until we reach a fixed point.
    // Note that at this point no underscores ('_') or counters ('$') should appear.
    bool done = false;
    while (!done) {
        done = true;

        // Try reducing each pair by one step
        for (std::size_t i = 0; i < sub.size(); i++) {
            auto currPair = sub[i];
            Argument* lhs = currPair.first;
            Argument* rhs = currPair.second;

            // Start trying to reduce the substitution
            // Note: Can probably go further with this substitution reduction
            if (*lhs == *rhs) {
                // Get rid of redundant `x = x`
                sub.erase(sub.begin() + i);
                done = false;
            } else if (isA<Constant>(lhs) && isA<Constant>(rhs)) {
                // Both are constants but not equal (prev case => !=)
                // Failed to unify!
                return false;
            } else if (isA<RecordInit>(lhs) && isA<RecordInit>(rhs)) {
                // Note: we will not deal with the case where only one side is
                // a record and the other is a variable, as variables can be records
                // on a deeper level.
                std::vector<Argument*> lhsArgs = static_cast<RecordInit*>(lhs)->getArguments();
                std::vector<Argument*> rhsArgs = static_cast<RecordInit*>(rhs)->getArguments();

                if (lhsArgs.size() != rhsArgs.size()) {
                    // Records of unequal size can't be equated
                    return false;
                }

                // Equate all corresponding arguments
                for (std::size_t i = 0; i < lhsArgs.size(); i++) {
                    sub.push_back(std::make_pair(lhsArgs[i], rhsArgs[i]));
                }

                // Get rid of the record equality
                sub.erase(sub.begin() + i);
                done = false;
            } else if ((isA<RecordInit>(lhs) && isA<Constant>(rhs)) ||
                       (isA<Constant>(lhs) && isA<RecordInit>(rhs))) {
                // A record =/= a constant
                return false;
            }
        }
    }

    return true;
}

/**
 * Returns the nullable vector of substitutions needed to unify the two given atoms.
 * If unification is not successful, the returned vector is marked as invalid.
 * Assumes that the atoms are both of the same relation.
 */
NullableVector<std::pair<Argument*, Argument*>> unifyAtoms(Atom* first, Atom* second) {
    std::vector<std::pair<Argument*, Argument*>> substitution;

    std::vector<Argument*> firstArgs = first->getArguments();
    std::vector<Argument*> secondArgs = second->getArguments();

    // Create the initial unification equalities
    for (std::size_t i = 0; i < firstArgs.size(); i++) {
        substitution.push_back(std::make_pair(firstArgs[i], secondArgs[i]));
    }

    // Reduce the substitutions
    bool success = reduceSubstitution(substitution);
    if (success) {
        return NullableVector<std::pair<Argument*, Argument*>>(substitution);
    } else {
        // Failed to unify the two atoms
        return NullableVector<std::pair<Argument*, Argument*>>();
    }
}

/**
 * Inlines the given atom based on a given clause.
 * Returns the vector of replacement literals and the necessary constraints.
 * If unification is unsuccessful, the vector of literals is marked as invalid.
 */
std::pair<NullableVector<Literal*>, std::vector<BinaryConstraint*>> inlineBodyLiterals(
        Atom& atom, Clause& atomInlineClause) {
    bool changed = false;
    std::vector<Literal*> addedLits;
    std::vector<BinaryConstraint*> constraints;

    // Rename the variables in the inlined clause to avoid conflicts when unifying multiple atoms
    // - particularly when an inlined relation appears twice in a clause.
    static int inlineCount = 0;

    // Make a temporary clone so we can rename variables without fear
    auto atomClause = clone(atomInlineClause);

    struct VariableRenamer : public NodeMapper {
        int varnum;
        VariableRenamer(int varnum) : varnum(varnum) {}
        Own<Node> operator()(Own<Node> node) const override {
            if (auto* var = as<ast::Variable>(node)) {
                // Rename the variable
                auto newVar = clone(var);
                std::stringstream newName;
                newName << "<inlined_" << var->getName() << "_" << varnum << ">";
                newVar->setName(newName.str());
                return newVar;
            }
            node->apply(*this);
            return node;
        }
    };

    VariableRenamer update(inlineCount);
    atomClause->apply(update);

    inlineCount++;

    // Get the constraints needed to unify th366Ge two atoms
    auto res = unifyAtoms(atomClause->getHead(), &atom);
    if (res.isValid()) {
        changed = true;
        for (std::pair<Argument*, Argument*> pair : res.getVector()) {
            // FIXME: float equiv (`FEQ`)
            constraints.push_back(
                    new BinaryConstraint(BinaryConstraintOp::EQ, clone(pair.first), clone(pair.second)));
        }

        // Add in the body of the current clause of the inlined atom
        for (Literal* lit : atomClause->getBodyLiterals()) {
            // FIXME: This is a horrible hack.  Should convert
            // addedLits to hold Own<>
            addedLits.push_back(clone(lit).release());
        }
    }

    if (changed) {
        return std::make_pair(NullableVector<Literal*>(addedLits), constraints);
    } else {
        return std::make_pair(NullableVector<Literal*>(), constraints);
    }
}

/**
 * Returns the negated version of a given literal
 */
Literal* negateLiteral(Literal* lit) {
    // FIXME: Ownership semantics
    if (auto* atom = as<Atom>(lit)) {
        auto* neg = new Negation(clone(atom));
        return neg;
    } else if (auto* neg = as<Negation>(lit)) {
        Atom* atom = clone(neg->getAtom()).release();
        return atom;
    } else if (auto* cons = as<Constraint>(lit)) {
        Constraint* newCons = clone(cons).release();
        negateConstraintInPlace(*newCons);
        return newCons;
    }

    fatal("unsupported literal type: %s", *lit);
}

/**
 * Return the negated version of a disjunction of conjunctions.
 * E.g. (a1(x) ^ a2(x) ^ ...) v (b1(x) ^ b2(x) ^ ...) --into-> (!a1(x) ^ !b1(x)) v (!a2(x) ^ !b2(x)) v ...
 */
std::vector<std::vector<Literal*>> combineNegatedLiterals(std::vector<std::vector<Literal*>> litGroups) {
    std::vector<std::vector<Literal*>> negation;

    // Corner case: !() = ()
    if (litGroups.empty()) {
        return negation;
    }

    std::vector<Literal*> litGroup = litGroups[0];
    if (litGroups.size() == 1) {
        // !(a1 ^ a2 ^ a3 ^ ...) --into-> !a1 v !a2 v !a3 v ...
        for (auto& i : litGroup) {
            std::vector<Literal*> newVec;
            newVec.push_back(negateLiteral(i));
            negation.push_back(newVec);
        }

        // Done!
        return negation;
    }

    // Produce the negated versions of all disjunctions ignoring the first recursively
    std::vector<std::vector<Literal*>> combinedRHS = combineNegatedLiterals(
            std::vector<std::vector<Literal*>>(litGroups.begin() + 1, litGroups.end()));

    // We now just need to add the negation of a single literal from the untouched LHS
    // to every single conjunction on the RHS to finalise creating every possible combination
    for (Literal* lhsLit : litGroup) {
        for (std::vector<Literal*> rhsVec : combinedRHS) {
            std::vector<Literal*> newVec;
            newVec.push_back(negateLiteral(lhsLit));

            for (Literal* lit : rhsVec) {
                // FIXME: This is a horrible hack.  Should convert
                // newVec to hold Own<>
                newVec.push_back(clone(lit).release());
            }

            negation.push_back(newVec);
        }
    }

    for (std::vector<Literal*> rhsVec : combinedRHS) {
        for (Literal* lit : rhsVec) {
            delete lit;
        }
    }

    return negation;
}

/**
 * Forms the bodies that will replace the negation of a given inlined atom.
 * E.g. a(x) <- (a11(x), a12(x)) ; (a21(x), a22(x)) => !a(x) <- (!a11(x), !a21(x)) ; (!a11(x), !a22(x)) ; ...
 * Essentially, produce every combination (m_1 ^ m_2 ^ ...) where m_i is the negation of a literal in the
 * ith rule of a.
 */
std::vector<std::vector<Literal*>> formNegatedLiterals(Program& program, Atom* atom) {
    // Constraints added to unify atoms should not be negated and should be added to
    // all the final rule combinations produced, and so should be stored separately.
    std::vector<std::vector<Literal*>> addedBodyLiterals;
    std::vector<std::vector<BinaryConstraint*>> addedConstraints;

    // Go through every possible clause associated with the given atom
    for (auto&& inClause : program.getClauses(atom->getQualifiedName())) {
        // Form the replacement clause by inlining based on the current clause
        auto [replacementBodyLiterals, currConstraints] = inlineBodyLiterals(*atom, *inClause);

        if (!replacementBodyLiterals.isValid()) {
            // Failed to unify, so just move on
            continue;
        }

        addedBodyLiterals.push_back(replacementBodyLiterals.getVector());
        addedConstraints.push_back(currConstraints);
    }

    // We now have a list of bodies needed to inline the given atom.
    // We want to inline the negated version, however, which is done using De Morgan's Law.
    std::vector<std::vector<Literal*>> negatedAddedBodyLiterals = combineNegatedLiterals(addedBodyLiterals);

    // Add in the necessary constraints to all the body literals
    for (auto& negatedAddedBodyLiteral : negatedAddedBodyLiterals) {
        for (std::vector<BinaryConstraint*> constraintGroup : addedConstraints) {
            for (BinaryConstraint* constraint : constraintGroup) {
                // FIXME: This is a horrible hack.  Should convert
                // negatedBodyLiterals to hold Own<>
                negatedAddedBodyLiteral.push_back(clone(constraint).release());
            }
        }
    }

    // Free up the old body literals and constraints
    for (std::vector<Literal*> litGroup : addedBodyLiterals) {
        for (Literal* lit : litGroup) {
            delete lit;
        }
    }
    for (std::vector<BinaryConstraint*> consGroup : addedConstraints) {
        for (Constraint* cons : consGroup) {
            delete cons;
        }
    }

    return negatedAddedBodyLiterals;
}

/**
 * Renames all variables in a given argument uniquely.
 */
void renameVariables(Argument* arg) {
    static int varCount = 0;
    varCount++;

    struct M : public NodeMapper {
        int varnum;
        M(int varnum) : varnum(varnum) {}
        Own<Node> operator()(Own<Node> node) const override {
            if (auto* var = as<ast::Variable>(node)) {
                auto newVar = clone(var);
                std::stringstream newName;
                newName << var->getName() << "-v" << varnum;
                newVar->setName(newName.str());
                return newVar;
            }
            node->apply(*this);
            return node;
        }
    };

    M update(varCount);
    arg->apply(update);
}

// Performs a given binary op on a list of aggregators recursively.
// E.g. ( <aggr1, aggr2, aggr3, ...>, o > = (aggr1 o (aggr2 o (agg3 o (...))))
// TODO (azreika): remove aggregator support
Argument* combineAggregators(std::vector<Aggregator*> aggrs, std::string fun) {
    // Due to variable scoping issues with aggregators, we rename all variables uniquely in the
    // added aggregator
    renameVariables(aggrs[0]);

    if (aggrs.size() == 1) {
        return aggrs[0];
    }

    Argument* rhs = combineAggregators(std::vector<Aggregator*>(aggrs.begin() + 1, aggrs.end()), fun);

    Argument* result = new IntrinsicFunctor(std::move(fun), Own<Argument>(aggrs[0]), Own<Argument>(rhs));

    return result;
}

/**
 * Returns a vector of arguments that should replace the given argument after one step of inlining.
 * Note: This function is currently generalised to perform any required inlining within aggregators
 * as well, making it simple to extend to this later on if desired (and the semantic check is removed).
 */
// TODO (azreika): rewrite this method, removing aggregators
NullableVector<Argument*> getInlinedArgument(Program& program, const Argument* arg) {
    bool changed = false;
    std::vector<Argument*> versions;

    // Each argument has to be handled differently - essentially, want to go down to
    // nested aggregators, and inline their bodies if needed.
    if (const auto* aggr = as<Aggregator>(arg)) {
        // First try inlining the target expression if necessary
        if (aggr->getTargetExpression() != nullptr) {
            NullableVector<Argument*> argumentVersions =
                    getInlinedArgument(program, aggr->getTargetExpression());

            if (argumentVersions.isValid()) {
                // An element in the target expression can be inlined!
                changed = true;

                // Create a new aggregator per version of the target expression
                for (Argument* newArg : argumentVersions.getVector()) {
                    auto* newAggr = new Aggregator(aggr->getBaseOperator(), Own<Argument>(newArg));
                    VecOwn<Literal> newBody;
                    for (Literal* lit : aggr->getBodyLiterals()) {
                        newBody.push_back(clone(lit));
                    }
                    newAggr->setBodyLiterals(std::move(newBody));
                    versions.push_back(newAggr);
                }
            }
        }

        // Try inlining body arguments if the target expression has not been changed.
        // (At this point we only handle one step of inlining at a time)
        if (!changed) {
            std::vector<Literal*> bodyLiterals = aggr->getBodyLiterals();
            for (std::size_t i = 0; i < bodyLiterals.size(); i++) {
                Literal* currLit = bodyLiterals[i];

                NullableVector<std::vector<Literal*>> literalVersions = getInlinedLiteral(program, currLit);

                if (literalVersions.isValid()) {
                    // Literal can be inlined!
                    changed = true;

                    AggregateOp op = aggr->getBaseOperator();

                    // Create an aggregator (with the same operation) for each possible body
                    std::vector<Aggregator*> aggrVersions;
                    for (std::vector<Literal*> inlineVersions : literalVersions.getVector()) {
                        Own<Argument> target;
                        if (aggr->getTargetExpression() != nullptr) {
                            target = clone(aggr->getTargetExpression());
                        }
                        auto* newAggr = new Aggregator(aggr->getBaseOperator(), std::move(target));

                        VecOwn<Literal> newBody;
                        // Add in everything except the current literal being replaced
                        for (std::size_t j = 0; j < bodyLiterals.size(); j++) {
                            if (i != j) {
                                newBody.push_back(clone(bodyLiterals[j]));
                            }
                        }

                        // Add in everything new that replaces that literal
                        for (Literal* addedLit : inlineVersions) {
                            newBody.push_back(Own<Literal>(addedLit));
                        }

                        newAggr->setBodyLiterals(std::move(newBody));
                        aggrVersions.push_back(newAggr);
                    }

                    // Utility lambda: get functor used to tie aggregators together.
                    auto aggregateToFunctor = [](AggregateOp op) {
                        switch (op) {
                            case AggregateOp::MIN:
                            case AggregateOp::FMIN:
                            case AggregateOp::UMIN: return "min";
                            case AggregateOp::MAX:
                            case AggregateOp::FMAX:
                            case AggregateOp::UMAX: return "max";
                            case AggregateOp::SUM:
                            case AggregateOp::FSUM:
                            case AggregateOp::USUM:
                            case AggregateOp::COUNT: return "+";
                            case AggregateOp::MEAN: fatal("no translation");
                        }

                        UNREACHABLE_BAD_CASE_ANALYSIS
                    };
                    // Create the actual overall aggregator that ties the replacement aggregators together.
                    // example: min x : { a(x) }. <=> min ( min x : { a1(x) }, min x : { a2(x) }, ... )
                    if (op != AggregateOp::MEAN) {
                        versions.push_back(combineAggregators(aggrVersions, aggregateToFunctor(op)));
                    }
                }

                // Only perform one stage of inlining at a time.
                if (changed) {
                    break;
                }
            }
        }
    } else if (const auto* functor = as<Functor>(arg)) {
        std::size_t i = 0;
        for (auto funArg : functor->getArguments()) {
            // TODO (azreika): use unique pointers
            // try inlining each argument from left to right
            NullableVector<Argument*> argumentVersions = getInlinedArgument(program, funArg);
            if (argumentVersions.isValid()) {
                changed = true;
                for (Argument* newArgVersion : argumentVersions.getVector()) {
                    // same functor but with new argument version
                    VecOwn<Argument> argsCopy;
                    std::size_t j = 0;
                    for (auto& functorArg : functor->getArguments()) {
                        if (j == i) {
                            argsCopy.emplace_back(newArgVersion);
                        } else {
                            argsCopy.emplace_back(clone(functorArg));
                        }
                        ++j;
                    }
                    if (const auto* intrFunc = as<IntrinsicFunctor>(arg)) {
                        auto* newFunctor = new IntrinsicFunctor(
                                intrFunc->getBaseFunctionOp(), std::move(argsCopy), functor->getSrcLoc());
                        versions.push_back(newFunctor);
                    } else if (const auto* userFunc = as<UserDefinedFunctor>(arg)) {
                        auto* newFunctor = new UserDefinedFunctor(
                                userFunc->getName(), std::move(argsCopy), userFunc->getSrcLoc());
                        versions.push_back(newFunctor);
                    }
                }
                // only one step at a time
                break;
            }
            ++i;
        }
    } else if (const auto* cast = as<ast::TypeCast>(arg)) {
        NullableVector<Argument*> argumentVersions = getInlinedArgument(program, cast->getValue());
        if (argumentVersions.isValid()) {
            changed = true;
            for (Argument* newArg : argumentVersions.getVector()) {
                Argument* newTypeCast = new ast::TypeCast(Own<Argument>(newArg), cast->getType());
                versions.push_back(newTypeCast);
            }
        }
    } else if (const auto* record = as<RecordInit>(arg)) {
        std::vector<Argument*> recordArguments = record->getArguments();
        for (std::size_t i = 0; i < recordArguments.size(); i++) {
            Argument* currentRecArg = recordArguments[i];
            NullableVector<Argument*> argumentVersions = getInlinedArgument(program, currentRecArg);
            if (argumentVersions.isValid()) {
                changed = true;
                for (Argument* newArgumentVersion : argumentVersions.getVector()) {
                    auto* newRecordArg = new RecordInit();
                    for (std::size_t j = 0; j < recordArguments.size(); j++) {
                        if (i == j) {
                            newRecordArg->addArgument(Own<Argument>(newArgumentVersion));
                        } else {
                            newRecordArg->addArgument(clone(recordArguments[j]));
                        }
                    }
                    versions.push_back(newRecordArg);
                }
            }

            // Only perform one stage of inlining at a time.
            if (changed) {
                break;
            }
        }
    }

    if (changed) {
        // Return a valid vector - replacements need to be made!
        return NullableVector<Argument*>(versions);
    } else {
        // Return an invalid vector - no inlining has occurred
        return NullableVector<Argument*>();
    }
}

/**
 * Returns a vector of atoms that should replace the given atom after one step of inlining.
 * Assumes the relation the atom belongs to is not inlined itself.
 */
NullableVector<Atom*> getInlinedAtom(Program& program, Atom& atom) {
    bool changed = false;
    std::vector<Atom*> versions;

    // Try to inline each of the atom's arguments
    std::vector<Argument*> arguments = atom.getArguments();
    for (std::size_t i = 0; i < arguments.size(); i++) {
        Argument* arg = arguments[i];

        NullableVector<Argument*> argumentVersions = getInlinedArgument(program, arg);

        if (argumentVersions.isValid()) {
            // Argument has replacements
            changed = true;

            // Create a new atom per new version of the argument
            for (Argument* newArgument : argumentVersions.getVector()) {
                auto args = atom.getArguments();
                VecOwn<Argument> newArgs;
                for (std::size_t j = 0; j < args.size(); j++) {
                    if (j == i) {
                        newArgs.emplace_back(newArgument);
                    } else {
                        newArgs.emplace_back(clone(args[j]));
                    }
                }
                auto* newAtom = new Atom(atom.getQualifiedName(), std::move(newArgs), atom.getSrcLoc());
                versions.push_back(newAtom);
            }
        }

        // Only perform one stage of inlining at a time.
        if (changed) {
            break;
        }
    }

    if (changed) {
        // Return a valid vector - replacements need to be made!
        return NullableVector<Atom*>(versions);
    } else {
        // Return an invalid vector - no replacements need to be made
        return NullableVector<Atom*>();
    }
}

/**
 * Tries to perform a single step of inlining on the given literal.
 * Returns a pair of nullable vectors (v, w) such that:
 *    - v is valid if and only if the literal can be directly inlined, whereby it
 *      contains the bodies that replace it
 *    - if v is not valid, then w is valid if and only if the literal cannot be inlined directly,
 *      but contains a subargument that can be. In this case, it will contain the versions
 *      that will replace it.
 *    - If both are invalid, then no more inlining can occur on this literal and we are done.
 */
NullableVector<std::vector<Literal*>> getInlinedLiteral(Program& program, Literal* lit) {
    bool inlined = false;
    bool changed = false;

    std::vector<std::vector<Literal*>> addedBodyLiterals;
    std::vector<Literal*> versions;

    if (auto* atom = as<Atom>(lit)) {
        // Check if this atom is meant to be inlined
        Relation* rel = program.getRelation(*atom);

        if (rel->hasQualifier(RelationQualifier::INLINE)) {
            // We found an atom in the clause that needs to be inlined!
            // The clause needs to be replaced
            inlined = true;

            // N new clauses should be formed, where N is the number of clauses
            // associated with the inlined relation
            for (auto&& inClause : program.getClauses(*rel)) {
                // Form the replacement clause
                auto [replacementBodyLiterals, currConstraints] = inlineBodyLiterals(*atom, *inClause);

                if (!replacementBodyLiterals.isValid()) {
                    // Failed to unify the atoms! We can skip this one...
                    continue;
                }

                // Unification successful - the returned vector of literals represents one possible body
                // replacement We can add in the unification constraints as part of these literals.
                std::vector<Literal*> bodyResult = replacementBodyLiterals.getVector();

                for (BinaryConstraint* cons : currConstraints) {
                    bodyResult.push_back(cons);
                }

                addedBodyLiterals.push_back(bodyResult);
            }
        } else {
            // Not meant to be inlined, but a subargument may be
            NullableVector<Atom*> atomVersions = getInlinedAtom(program, *atom);
            if (atomVersions.isValid()) {
                // Subnode needs to be inlined, so we have a vector of replacement atoms
                changed = true;
                for (Atom* newAtom : atomVersions.getVector()) {
                    versions.push_back(newAtom);
                }
            }
        }
    } else if (auto neg = as<Negation>(lit)) {
        // For negations, check the corresponding atom
        Atom* atom = neg->getAtom();
        NullableVector<std::vector<Literal*>> atomVersions = getInlinedLiteral(program, atom);

        if (atomVersions.isValid()) {
            // The atom can be inlined
            inlined = true;

            if (atomVersions.getVector().empty()) {
                // No clauses associated with the atom, so just becomes a true literal
                addedBodyLiterals.push_back({new BooleanConstraint(true)});
            } else {
                // Suppose an atom a(x) is inlined and has the following rules:
                //  - a(x) :- a11(x), a12(x).
                //  - a(x) :- a21(x), a22(x).
                // Then, a(x) <- (a11(x) ^ a12(x)) v (a21(x) ^ a22(x))
                //  => !a(x) <- (!a11(x) v !a12(x)) ^ (!a21(x) v !a22(x))
                //  => !a(x) <- (!a11(x) ^ !a21(x)) v (!a11(x) ^ !a22(x)) v ...
                // Essentially, produce every combination (m_1 ^ m_2 ^ ...) where m_i is a
                // negated literal in the ith rule of a.
                addedBodyLiterals = formNegatedLiterals(program, atom);
            }
        }
        if (atomVersions.isValid()) {
            for (const auto& curVec : atomVersions.getVector()) {
                for (auto* cur : curVec) {
                    delete cur;
                }
            }
        }
    } else if (auto* constraint = as<BinaryConstraint>(lit)) {
        NullableVector<Argument*> lhsVersions = getInlinedArgument(program, constraint->getLHS());
        if (lhsVersions.isValid()) {
            changed = true;
            for (Argument* newLhs : lhsVersions.getVector()) {
                Literal* newLit = new BinaryConstraint(
                        constraint->getBaseOperator(), Own<Argument>(newLhs), clone(constraint->getRHS()));
                versions.push_back(newLit);
            }
        } else {
            NullableVector<Argument*> rhsVersions = getInlinedArgument(program, constraint->getRHS());
            if (rhsVersions.isValid()) {
                changed = true;
                for (Argument* newRhs : rhsVersions.getVector()) {
                    Literal* newLit = new BinaryConstraint(constraint->getBaseOperator(),
                            clone(constraint->getLHS()), Own<Argument>(newRhs));
                    versions.push_back(newLit);
                }
            }
        }
    }

    if (changed) {
        // Not inlined directly but found replacement literals
        // Rewrite these as single-literal bodies
        for (Literal* version : versions) {
            std::vector<Literal*> newBody;
            newBody.push_back(version);
            addedBodyLiterals.push_back(newBody);
        }
        inlined = true;
    }

    if (inlined) {
        return NullableVector<std::vector<Literal*>>(addedBodyLiterals);
    } else {
        return NullableVector<std::vector<Literal*>>();
    }
}

/**
 * Returns a list of clauses that should replace the given clause after one step of inlining.
 * If no inlining can occur, the list will only contain a clone of the original clause.
 */
std::vector<Clause*> getInlinedClause(Program& program, const Clause& clause) {
    bool changed = false;
    std::vector<Clause*> versions;

    // Try to inline things contained in the arguments of the head first.
    // E.g. `a(x, max y : { b(y) }) :- c(x).`, where b should be inlined.
    Atom* head = clause.getHead();
    NullableVector<Atom*> headVersions = getInlinedAtom(program, *head);
    if (headVersions.isValid()) {
        // The head atom can be inlined!
        changed = true;

        // Produce the new clauses with the replacement head atoms
        for (Atom* newHead : headVersions.getVector()) {
            auto newClause = mk<Clause>(Own<Atom>(newHead), clause.getSrcLoc());
            newClause->setBodyLiterals(clone(clause.getBodyLiterals()));

            // FIXME: tomp - hack - this should be managed
            versions.push_back(newClause.release());
        }
    }

    // Only perform one stage of inlining at a time.
    // If the head atoms did not need inlining, try inlining atoms nested in the body.
    if (!changed) {
        std::vector<Literal*> bodyLiterals = clause.getBodyLiterals();
        for (std::size_t i = 0; i < bodyLiterals.size(); i++) {
            Literal* currLit = bodyLiterals[i];

            // Three possible cases when trying to inline a literal:
            //  1) The literal itself may be directly inlined. In this case, the atom can be replaced
            //    with multiple different bodies, as the inlined atom may have several rules.
            //  2) Otherwise, the literal itself may not need to be inlined, but a subnode (e.g. an argument)
            //    may need to be inlined. In this case, an altered literal must replace the original.
            //    Again, several possible versions may exist, as the inlined relation may have several rules.
            //  3) The literal does not depend on any inlined relations, and so does not need to be changed.
            NullableVector<std::vector<Literal*>> litVersions = getInlinedLiteral(program, currLit);

            if (litVersions.isValid()) {
                // Case 1 and 2: Inlining has occurred!
                changed = true;

                // The literal may be replaced with several different bodies.
                // Create a new clause for each possible version.
                std::vector<std::vector<Literal*>> bodyVersions = litVersions.getVector();

                // Create the base clause with the current literal removed
                auto baseClause = Own<Clause>(clause.cloneHead());

                for (std::vector<Literal*> const& body : bodyVersions) {
                    auto replacementClause = clone(baseClause);
                    // insert literals appearing before the one inlined
                    for (Literal* oldLit : bodyLiterals) {
                        if (currLit != oldLit) {
                            replacementClause->addToBody(clone(oldLit));
                        } else {
                            break;
                        }
                    }

                    // Add in the current set of literals replacing the inlined literal
                    // In Case 2, each body contains exactly one literal
                    replacementClause->addToBody(VecOwn<Literal>(body.begin(), body.end()));

                    // insert literals appearing after the one inlined
                    bool seen = false;
                    for (Literal* oldLit : bodyLiterals) {
                        if (currLit != oldLit && seen) {
                            replacementClause->addToBody(clone(oldLit));
                        } else if (currLit == oldLit) {
                            seen = true;
                        }
                    }

                    // FIXME: This is a horrible hack.  Should convert
                    // versions to hold Own<>
                    versions.push_back(replacementClause.release());
                }
            }

            // Only replace at most one literal per iteration
            if (changed) {
                break;
            }
        }
    }

    if (!changed) {
        // Case 3: No inlining changes, so a clone of the original should be returned
        // FIXME: This is a horrible hack.  Should convert
        // res to hold Own<>
        std::vector<Clause*> ret;
        ret.push_back(clone(clause).release());
        return ret;
    } else {
        // Inlining changes, so return the replacement clauses.
        return versions;
    }
}

ExcludedRelations InlineRelationsTransformer::excluded() {
    ExcludedRelations xs;
    auto addAll = [&](const std::string& name) {
        for (auto&& r : splitString(Global::config().get(name), ','))
            xs.insert(QualifiedName(r));
    };

    addAll("inline-exclude");
    addAll("magic-transform-exclude");
    return xs;
}

bool InlineRelationsTransformer::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    // Replace constants in the head of inlined clauses with (constrained) variables.
    // This is done to simplify atom unification, particularly when negations are involved.
    changed |= normaliseInlinedHeads(program);

    // Remove underscores in inlined atoms in the program to avoid issues during atom unification
    changed |= nameInlinedUnderscores(program);

    // Keep trying to inline things until we reach a fixed point.
    // Since we know there are no cyclic dependencies between inlined relations, this will necessarily
    // terminate.
    bool clausesChanged = true;
    while (clausesChanged) {
        clausesChanged = false;

        // Go through each relation in the program and check if we need to inline any of its clauses
        for (Relation* rel : program.getRelations()) {
            // Skip if the relation is going to be inlined or if no_inline or no_magic is present
            if (rel->hasQualifier(RelationQualifier::INLINE) ||
                    rel->hasQualifier(RelationQualifier::NO_INLINE) ||
                    rel->hasQualifier(RelationQualifier::NO_MAGIC)) {
                continue;
            }

            // Go through the relation's clauses and try inlining them
            for (auto&& clause : program.getClauses(*rel)) {
                if (containsInlinedAtom(program, *clause)) {
                    // Generate the inlined versions of this clause - the clause will be replaced by these
                    for (auto* replacementClause : getInlinedClause(program, *clause)) {
                        program.addClause(Own<Clause>(replacementClause));
                    }

                    // Remove old clause
                    program.removeClause(*clause);

                    // We've changed the program this iteration
                    clausesChanged = true;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

bool InlineUnmarkExcludedTransform::transform(TranslationUnit& translationUnit) {
    bool changed = false;
    Program& program = translationUnit.getProgram();

    auto&& excluded = InlineRelationsTransformer::excluded();

    for (Relation* rel : program.getRelations()) {
        // no-magic implies no inlining
        auto exclude = contains(excluded, rel->getQualifiedName()) ||
                       rel->hasQualifier(RelationQualifier::NO_INLINE) ||
                       rel->hasQualifier(RelationQualifier::NO_MAGIC);
        if (exclude) {
            changed |= rel->removeQualifier(RelationQualifier::INLINE);
            changed |= rel->addQualifier(RelationQualifier::NO_INLINE);
        }
    }

    return changed;
}

}  // namespace souffle::ast::transform
