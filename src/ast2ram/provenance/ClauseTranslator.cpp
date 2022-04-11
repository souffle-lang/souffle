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

#include "ast2ram/provenance/ClauseTranslator.h"
#include "Global.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/GuardedInsert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/Negation.h"
#include "ram/Operation.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Scan.h"
#include "ram/SignedConstant.h"
#include "ram/UndefValue.h"
#include "souffle/utility/StringUtil.h"

namespace souffle::ast2ram::provenance {

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
}

Own<ram::Operation> ClauseTranslator::addNegatedAtom(
        Own<ram::Operation> op, const ast::Clause& clause, const ast::Atom* atom) const {
    VecOwn<ram::Expression> values;

    auto args = atom->getArguments();
    for (const auto* arg : args) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    // undefined value for rule number
    values.push_back(mk<ram::UndefValue>());

    // height
    values.push_back(getLevelNumber(clause));

    return mk<ram::Filter>(mk<ram::Negation>(mk<ram::ProvenanceExistenceCheck>(
                                   getConcreteRelationName(atom->getQualifiedName()), std::move(values))),
            std::move(op));
}

void ClauseTranslator::indexAtoms(const ast::Clause& clause) {
    std::size_t atomIdx = 0;
    for (const auto* atom : getAtomOrdering(clause)) {
        // give the atom the current level
        std::size_t scanLevel = addOperatorLevel(atom);
        indexNodeArguments(scanLevel, atom->getArguments());

        // Add rule num variable
        std::string ruleNumVarName = "@rule_num_" + std::to_string(atomIdx);
        valueIndex->addVarReference(ruleNumVarName, scanLevel, atom->getArity());

        // Add level num variable
        std::string levelNumVarName = "@level_num_" + std::to_string(atomIdx);
        valueIndex->addVarReference(levelNumVarName, scanLevel, atom->getArity() + 1);

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

Own<ram::Operation> ClauseTranslator::addAtomScan(Own<ram::Operation> op, const ast::Atom* atom,
        const ast::Clause& clause, std::size_t curLevel) const {
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
        values.push_back(mk<ram::SignedConstant>(context.getClauseNum(&clause)));
        values.push_back(getLevelNumber(clause));
    }

    // Relations with functional dependency constraints
    if (auto guardedConditions = getFunctionalDependencies(clause)) {
        return mk<ram::GuardedInsert>(headRelationName, std::move(values), std::move(guardedConditions));
    }

    // Everything else
    return mk<ram::Insert>(headRelationName, std::move(values));
}

}  // namespace souffle::ast2ram::provenance
