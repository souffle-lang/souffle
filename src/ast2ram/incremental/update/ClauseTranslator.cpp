/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseTranslator.cpp
 *
 ***********************************************************************/

#include "ast2ram/incremental/update/ClauseTranslator.h"
#include "Global.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "ast2ram/utility/Location.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ast2ram/incremental/Utils.h"
#include "ast2ram/seminaive/ClauseTranslator.h"
#include "ram/Aggregate.h"
#include "ram/Constraint.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/GuardedInsert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/IterationNumber.h"
#include "ram/Negation.h"
#include "ram/Operation.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "souffle/utility/StringUtil.h"

namespace souffle::ast2ram::incremental::update {

/*
void ClauseTranslator::setDiffVersion(std::size_t diffVersion) {
    this->diffVersion = diffVersion;
}
*/

Own<ram::Statement> ClauseTranslator::translateNonRecursiveClause(const ast::Clause& clause) {

    VecOwn<ram::Statement> result;

    for (std::size_t diffVersion = 0; diffVersion < ast::getBodyLiterals<ast::Atom>(clause).size(); diffVersion++) {
        // Update diffVersion config
        this->diffVersion = diffVersion;

        appendStmt(result, seminaive::ClauseTranslator::translateNonRecursiveClause(clause));
    }

    this->diffVersion = 0;

    return mk<ram::Sequence>(std::move(result));
}

Own<ram::Statement> ClauseTranslator::translateRecursiveClause(
        const ast::Clause& clause, const std::set<const ast::Relation*>& scc, std::size_t version) {
    // Update diffVersion config
    this->diffVersion = 0;

    return seminaive::ClauseTranslator::translateRecursiveClause(clause, scc, version);
}

Own<ram::Operation> ClauseTranslator::addNegatedDeltaAtom(
        Own<ram::Operation> op, const ast::Atom* atom) const {
    std::size_t arity = atom->getArity();
    std::string name = getDeltaRelationName(atom->getQualifiedName());

    if (arity == 0) {
        // for a nullary, negation is a simple emptiness check
        return mk<ram::Filter>(mk<ram::EmptinessCheck>(name), std::move(op));
    }

    // else, we construct the atom and create a negation
    VecOwn<ram::Expression> values;
    auto args = atom->getArguments();
    for (const auto* arg : args) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }
    values.push_back(mk<ram::UndefValue>());
    values.push_back(mk<ram::UndefValue>());

    return mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::ExistenceCheck>(name, std::move(values))), std::move(op));

    // For incremental evaluation, the delta check is enforced during merge time so does not need to be checked during rule evaluation
    // return nullptr;
}

std::string ClauseTranslator::getClauseAtomName(const ast::Clause& clause, const ast::Atom* atom) const {
    /* TODO: incremental does not currently work with subsumptive clauses
    if (isA<ast::SubsumptiveClause>(clause)) {
        // find the dominated / dominating heads
        const auto& body = clause.getBodyLiterals();
        auto dominatedHeadAtom = dynamic_cast<const ast::Atom*>(body[0]);
        auto dominatingHeadAtom = dynamic_cast<const ast::Atom*>(body[1]);

        if (clause.getHead() == atom) {
            if (mode == SubsumeDeleteCurrentDelta || mode == SubsumeDeleteCurrentCurrent) {
                return getDeleteRelationName(atom->getQualifiedName());
            }
            return getRejectRelationName(atom->getQualifiedName());
        }

        if (dominatedHeadAtom == atom) {
            if (mode == SubsumeDeleteCurrentDelta || mode == SubsumeDeleteCurrentCurrent) {
                return getConcreteRelationName(atom->getQualifiedName());
            }
            return getNewRelationName(atom->getQualifiedName());
        }

        if (dominatingHeadAtom == atom) {
            switch (mode) {
                case SubsumeRejectNewCurrent:
                case SubsumeDeleteCurrentCurrent: return getConcreteRelationName(atom->getQualifiedName());
                case SubsumeDeleteCurrentDelta: return getDeltaRelationName(atom->getQualifiedName());
                default: return getNewRelationName(atom->getQualifiedName());
            }
        }

        if (isRecursive()) {
            if (sccAtoms.at(version + 1) == atom) {
                return getDeltaRelationName(atom->getQualifiedName());
            }
        }
    }
    */

    if (!isRecursive()) {
        if (ast::getBodyLiterals<ast::Atom>(clause).at(diffVersion) == atom ||
                clause.getHead() == atom) {
            if (mode == IncrementalDiffPlus) {
                return getDiffPlusRelationName(atom->getQualifiedName());
            } else if (mode == IncrementalDiffMinus) {
                return getDiffMinusRelationName(atom->getQualifiedName());
            }
        }

        if (mode == IncrementalDiffPlus) {
            return getConcreteRelationName(atom->getQualifiedName());
        } else if (mode == IncrementalDiffMinus) {
            return getPrevRelationName(atom->getQualifiedName());
        }
    }
    if (clause.getHead() == atom) {
        return getNewRelationName(atom->getQualifiedName());
    }
    if (sccAtoms.at(version) == atom) {
        return getDeltaRelationName(atom->getQualifiedName());
    }
    return getConcreteRelationName(atom->getQualifiedName());
}

Own<ram::Operation> ClauseTranslator::addNegatedAtom(
        Own<ram::Operation> op, const ast::Clause& /* clause */, const ast::Atom* /* atom */) const {
    /*
    VecOwn<ram::Expression> values;

    auto args = atom->getArguments();
    for (const auto* arg : args) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    // undefined value for rule number
    values.push_back(mk<ram::UndefValue>());

    // height
    // TODO: i need to work out what this is lol
    values.push_back(mk<ram::UndefValue>());

    return mk<ram::Filter>(mk<ram::Negation>(mk<ram::ProvenanceExistenceCheck>(
                                   getConcreteRelationName(atom->getQualifiedName()), std::move(values))),
            std::move(op));
            */

    return op;
}

Own<ram::Operation> ClauseTranslator::addBodyLiteralConstraints(
        const ast::Clause& clause, Own<ram::Operation> op) const {

    op = ast2ram::seminaive::ClauseTranslator::addBodyLiteralConstraints(clause, std::move(op));

    std::size_t lastScanLevel = 0;
    while (valueIndex->isSomethingDefinedOn(lastScanLevel)) {
        lastScanLevel++;
    }

    // For incremental update, ensure that some body tuples exist in both the
    // previous and current epochs to prevent double counting. For example,
    // consider the rule
    //
    //   R :- S, T.
    //
    // This generates two diff versions
    //
    //   diff_minus_R :- diff_minus_S, prev_T.
    //   diff_minus_R :- prev_S, diff_minus_T.
    //
    // We need to ensure that for the second case, the tuple for prev_S also
    // exists in S, otherwise it would be double counting tuples in S that are
    // removed. To achieve this, add an extra constraint:
    //
    //   FOR t0 in prev_S:
    //     FOR t1 in diff_minus_T:
    //       IF EXISTS t0 in S WHERE t0.count > 0
    //         ...

    std::size_t tupleExistsLevel = lastScanLevel;
    for (std::size_t i = 0; i < diffVersion; i++) {
        // TODO: check how getAtomOrdering works with diffVersions
        const auto* atom = getAtomOrdering(clause)[i];

        // First create a filter condition
        Own<ram::Condition> tupleExistsCond = mk<ram::Constraint>(BinaryConstraintOp::GT, mk<ram::TupleElement>(tupleExistsLevel, atom->getArity() + 1), mk<ram::SignedConstant>(0));

        // Add conditions to say the tuple must match
        std::size_t argNum = 0;
        for (auto* arg : atom->getArguments()) {
            tupleExistsCond = addConjunctiveTerm(std::move(tupleExistsCond), mk<ram::Constraint>(BinaryConstraintOp::EQ, context.translateValue(*valueIndex, arg), mk<ram::TupleElement>(tupleExistsLevel, argNum)));
            argNum++;
        }

        // Create a filter
        op = mk<ram::Filter>(std::move(tupleExistsCond), std::move(op));

        // Create a scan
        if (mode == IncrementalDiffPlus) {
            op = mk<ram::Scan>(getPrevRelationName(atom->getQualifiedName()), tupleExistsLevel, std::move(op));
        } else if (mode == IncrementalDiffMinus) {
            op = mk<ram::Scan>(getConcreteRelationName(atom->getQualifiedName()), tupleExistsLevel, std::move(op));
        }

        tupleExistsLevel++;
    }

    lastScanLevel = tupleExistsLevel;


    // Ensure that we only compute using the earliest iteration for any given
    // tuple. For example, given a binary relation (x,y,@iter,@count), if we
    // have two tuples (1,2,3,1) and (1,2,5,1), we should only execute a rule
    // evaluation for the earliest tuple (1,2,3,1), otherwise incremental
    // evaluation double counts.

    // atomIdx keeps track of which atom we are creating an aggregate for
    std::size_t atomIdx = 0;

    // aggLevel denotes the current level for aggregation, it starts after all
    // atoms in the rule
    std::size_t aggLevel = lastScanLevel;
    for (const auto* atom : getAtomOrdering(clause)) {
        // For an atom A(X...,iter,count), generate an aggregate constraint
        // iter = i : min(A(X...,i,c WHERE c > 0))

        std::string iterationNum = "@min_iteration_" + std::to_string(atomIdx);
        valueIndex->addVarReference(iterationNum, aggLevel, atom->getArity());

        // std::string iterationCountNum = "@min_iteration_count_" + std::to_string(atomIdx);
        // valueIndex->addVarReference(iterationCountNum, aggLevel, atom->getArity() + 1);

        // Make aggregate expression, i.e., i in the above example
        auto aggExpr = mk<ram::TupleElement>(aggLevel, atom->getArity());

        // Make aggregate condition, i.e., c > 0 in above example
        Own<ram::Condition> aggCond = mk<ram::Constraint>(BinaryConstraintOp::GT, mk<ram::TupleElement>(aggLevel, atom->getArity() + 1), mk<ram::SignedConstant>(0));

        // Add aggregate conditions such that X... inside aggregate is equal to outside
        std::size_t argNum = 0;
        for (auto* arg : atom->getArguments()) {
            aggCond = addConjunctiveTerm(std::move(aggCond), mk<ram::Constraint>(BinaryConstraintOp::EQ, context.translateValue(*valueIndex, arg), mk<ram::TupleElement>(aggLevel, argNum)));
            argNum++;
        }

        auto iterationVariableLocation = valueIndex->getDefinitionPoint("@iteration_" + std::to_string(atomIdx));

        op = mk<ram::Filter>(mk<ram::Constraint>(BinaryConstraintOp::EQ,
                    mk<ram::TupleElement>(iterationVariableLocation.identifier, iterationVariableLocation.element),
                    mk<ram::TupleElement>(aggLevel, 0)), std::move(op));

        // Make aggregate, i.e., i : min(A(X...,i,c WHERE c > 0)) in above example
        // This has to go after the filter, because the op is being built inside out
        if (mode == IncrementalDiffPlus) {
            op = mk<ram::Aggregate>(std::move(op), AggregateOp::MIN, getConcreteRelationName(atom->getQualifiedName()), std::move(aggExpr), std::move(aggCond), aggLevel);
        } else if (mode == IncrementalDiffMinus) {
            op = mk<ram::Aggregate>(std::move(op), AggregateOp::MIN, getPrevRelationName(atom->getQualifiedName()), std::move(aggExpr), std::move(aggCond), aggLevel);
        }

        atomIdx++;
        aggLevel++;
    }

    return op;
}

void ClauseTranslator::indexAtoms(const ast::Clause& clause) {
    std::size_t atomIdx = 0;
    for (const auto* atom : getAtomOrdering(clause)) {
        // give the atom the current level
        int scanLevel = addOperatorLevel(atom);
        indexNodeArguments(scanLevel, atom->getArguments());

        // Add rule num variable
        std::string iterationNumVarName = "@iteration_" + std::to_string(atomIdx);
        valueIndex->addVarReference(iterationNumVarName, scanLevel, atom->getArity());

        // Add level num variable
        std::string countNumVarName = "@count_" + std::to_string(atomIdx);
        valueIndex->addVarReference(countNumVarName, scanLevel, atom->getArity() + 1);

        atomIdx++;
    }
}

Own<ram::Expression> ClauseTranslator::getLevelNumber(const ast::Clause& clause) const {
    auto getLevelVariable = [&](std::size_t atomIdx) { return "@level_num_" + std::to_string(atomIdx); };

    const auto& bodyAtoms = getAtomOrdering(clause);
    if (bodyAtoms.empty()) return mk<ram::SignedConstant>(0);

    VecOwn<ram::Expression> values;
    for (std::size_t i = 0; i < bodyAtoms.size(); i++) {
        auto levelVar = mk<ast::Variable>(getLevelVariable(i));
        values.push_back(context.translateValue(*valueIndex, levelVar.get()));
    }
    assert(!values.empty() && "unexpected empty value set");

    auto maxLevel = values.size() == 1 ? std::move(values.at(0))
                                       : mk<ram::IntrinsicOperator>(FunctorOp::MAX, std::move(values));

    VecOwn<ram::Expression> addArgs;
    addArgs.push_back(std::move(maxLevel));
    addArgs.push_back(mk<ram::SignedConstant>(1));
    return mk<ram::IntrinsicOperator>(FunctorOp::ADD, std::move(addArgs));
}

Own<ram::Operation> ClauseTranslator::addAtomScan(
        Own<ram::Operation> op, const ast::Atom* atom, const ast::Clause& clause, int curLevel) const {
    // add constraints
    op = addConstantConstraints(curLevel, atom->getArguments(), std::move(op));

    // add check for emptiness for an atom
    op = mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::EmptinessCheck>(getClauseAtomName(clause, atom))), std::move(op));

    // add a scan level
    std::stringstream ss;
    if (Global::config().has("profile")) {
        ss << "@frequency-atom" << ';';
        ss << clause.getHead()->getQualifiedName() << ';';
        ss << version << ';';
        ss << stringify(getClauseString(clause)) << ';';
        ss << stringify(getClauseAtomName(clause, atom)) << ';';
        ss << stringify(toString(clause)) << ';';
        ss << curLevel << ';';
    }
    op = mk<ram::Scan>(getClauseAtomName(clause, atom), curLevel, std::move(op), ss.str());

    return op;
}

Own<ram::Operation> ClauseTranslator::createInsertion(const ast::Clause& clause) const {
    const auto head = clause.getHead();
    auto headRelationName = getClauseAtomName(clause, head);

    VecOwn<ram::Expression> values;
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    // add rule number + level number
    if (isFact(clause)) {
        values.push_back(mk<ram::SignedConstant>(0));
        values.push_back(mk<ram::SignedConstant>(0));
    } else {
        values.push_back(mk<ram::IterationNumber>());
        if (mode == IncrementalDiffPlus) {
            values.push_back(mk<ram::SignedConstant>(1));
        } else if (mode == IncrementalDiffMinus) {
            values.push_back(mk<ram::SignedConstant>(-1));
        } else {
            std::cerr << "incremental update translate mode should be either diff_plus or diff_minus, never default" << std::endl;
            values.push_back(mk<ram::SignedConstant>(1));
        }
    }

    // Relations with functional dependency constraints
    if (auto guardedConditions = getFunctionalDependencies(clause)) {
        return mk<ram::GuardedInsert>(headRelationName, std::move(values), std::move(guardedConditions));
    }

    // Everything else
    return mk<ram::Insert>(headRelationName, std::move(values));
}

}  // namespace souffle::ast2ram::incremental::update
