/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseTranslator.h
 *
 * Translator for clauses from AST to RAM
 *
 ***********************************************************************/

#include "ast2ram/seminaive/ClauseTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "RelationTag.h"
#include "ast/Aggregator.h"
#include "ast/BranchInit.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/NilConstant.h"
#include "ast/NumericConstant.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/StringConstant.h"
#include "ast/SubsumptiveClause.h"
#include "ast/UnnamedVariable.h"
#include "ast/analysis/Functor.h"
#include "ast/utility/SipsMetric.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/Location.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/Aggregate.h"
#include "ram/Break.h"
#include "ram/Constraint.h"
#include "ram/CountUniqueKeys.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/AggregateExistenceCheck.h"
#include "ram/Filter.h"
#include "ram/FloatConstant.h"
#include "ram/GuardedInsert.h"
#include "ram/Insert.h"
#include "ram/LogRelationTimer.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/Query.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/StringConstant.h"
#include "ram/TupleElement.h"
#include "ram/UnpackRecord.h"
#include "ram/UnsignedConstant.h"
#include "ram/utility/Utils.h"
#include "souffle/utility/StringUtil.h"
#include <map>
#include <unordered_set>
#include <vector>

namespace souffle::ast2ram::seminaive {

ClauseTranslator::ClauseTranslator(const TranslatorContext& context, TranslationMode mode)
        : ast2ram::ClauseTranslator(context, mode), valueIndex(mk<ValueIndex>()) {}

ClauseTranslator::~ClauseTranslator() = default;

bool ClauseTranslator::isRecursive() const {
    return !sccAtoms.empty();
}

std::string ClauseTranslator::getClauseString(const ast::Clause& clause) const {
    auto renamedClone = clone(clause);

    // Update the head atom
    renamedClone->getHead()->setQualifiedName(getClauseAtomName(clause, clause.getHead()));

    // Update the body atoms
    const auto& cloneAtoms = ast::getBodyLiterals<ast::Atom>(*renamedClone);
    const auto& originalAtoms = ast::getBodyLiterals<ast::Atom>(clause);
    assert(originalAtoms.size() == cloneAtoms.size() && "clone should have same atoms");
    for (std::size_t i = 0; i < cloneAtoms.size(); i++) {
        auto cloneAtom = cloneAtoms.at(i);
        const auto* originalAtom = originalAtoms.at(i);
        assert(originalAtom->getQualifiedName() == cloneAtom->getQualifiedName() &&
                "atom sequence in clone should match");
        cloneAtom->setQualifiedName(getClauseAtomName(clause, originalAtom));
    }

    return toString(*renamedClone);
}

Own<ram::Statement> ClauseTranslator::translateRecursiveClause(
        const ast::Clause& clause, const std::set<const ast::Relation*>& scc, std::size_t version) {
    // Update version config
    sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(clause),
            [&](auto* atom) { return contains(scc, context.getProgram()->getRelation(*atom)); });
    this->version = version;

    // Translate the resultant clause as would be done normally
    Own<ram::Statement> rule = translateNonRecursiveClause(clause);

    // Add logging
    if (Global::config().has("profile")) {
        const std::string& relationName = getConcreteRelationName(clause.getHead()->getQualifiedName());
        const auto& srcLocation = clause.getSrcLoc();
        const std::string clauseText = stringify(toString(clause));
        const std::string logTimerStatement =
                LogStatement::tRecursiveRule(relationName, version, srcLocation, clauseText);
        const std::string logSizeStatement =
                LogStatement::nRecursiveRule(relationName, version, srcLocation, clauseText);
        rule = mk<ram::LogRelationTimer>(
                std::move(rule), logTimerStatement, getClauseAtomName(clause, clause.getHead()));
    }

    // Add debug info
    std::ostringstream ds;
    ds << toString(clause) << "\nin file ";
    ds << clause.getSrcLoc();
    rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

    // Add to loop body
    return mk<ram::Sequence>(std::move(rule));
}

Own<ram::Statement> ClauseTranslator::translateNonRecursiveClause(const ast::Clause& clause) {
    // Create the appropriate query
    if (isFact(clause)) {
        return createRamFactQuery(clause);
    }
    return createRamRuleQuery(clause);
}

std::string ClauseTranslator::getClauseAtomName(const ast::Clause& clause, const ast::Atom* atom) const {
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

    if (!isRecursive()) {
        return getConcreteRelationName(atom->getQualifiedName());
    }
    if (clause.getHead() == atom) {
        return getNewRelationName(atom->getQualifiedName());
    }
    if (sccAtoms.at(version) == atom) {
        return getDeltaRelationName(atom->getQualifiedName());
    }
    return getConcreteRelationName(atom->getQualifiedName());
}

Own<ram::Statement> ClauseTranslator::createRamFactQuery(const ast::Clause& clause) const {
    assert(isFact(clause) && "clause should be fact");
    assert(!isRecursive() && "recursive clauses cannot have facts");

    // Create a fact statement
    return mk<ram::Query>(createInsertion(clause));
}

Own<ram::Statement> ClauseTranslator::createRamRuleQuery(const ast::Clause& clause) {
    assert(isRule(clause) && "clause should be rule");

    // Index all variables and generators in the clause
    indexClause(clause);

    // Set up the RAM statement bottom-up
    auto op = createInsertion(clause);
    op = addBodyLiteralConstraints(clause, std::move(op));
    op = addVariableBindingConstraints(std::move(op));
    op = addGeneratorLevels(std::move(op), clause);
    op = addVariableIntroductions(clause, std::move(op));
    op = addEntryPoint(clause, std::move(op));
    return mk<ram::Query>(std::move(op));
}

Own<ram::Operation> ClauseTranslator::addEntryPoint(const ast::Clause& clause, Own<ram::Operation> op) const {
    auto cond = createCondition(clause);
    return cond != nullptr ? mk<ram::Filter>(std::move(cond), std::move(op)) : std::move(op);
}

Own<ram::Operation> ClauseTranslator::addVariableBindingConstraints(Own<ram::Operation> op) const {
    for (const auto& [_, references] : valueIndex->getVariableReferences()) {
        // Equate the first appearance to all other appearances
        assert(!references.empty() && "variable should appear at least once");
        const auto& first = *references.begin();
        for (const auto& reference : references) {
            if (first != reference && !valueIndex->isGenerator(reference.identifier)) {
                // TODO: float type equivalence check
                op = addEqualityCheck(
                        std::move(op), makeRamTupleElement(first), makeRamTupleElement(reference), false);
            }
        }
    }
    return op;
}

Own<ram::Operation> ClauseTranslator::createInsertion(const ast::Clause& clause) const {
    const auto head = clause.getHead();
    auto headRelationName = getClauseAtomName(clause, head);

    VecOwn<ram::Expression> values;
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    // Propositions
    if (head->getArity() == 0) {
        return mk<ram::Filter>(mk<ram::EmptinessCheck>(headRelationName),
                mk<ram::Insert>(headRelationName, std::move(values)));
    }

    // Relations with functional dependency constraints
    if (auto guardedConditions = getFunctionalDependencies(clause)) {
        return mk<ram::GuardedInsert>(headRelationName, std::move(values), std::move(guardedConditions));
    }

    // Everything else
    return mk<ram::Insert>(headRelationName, std::move(values));
}

Own<ram::Operation> ClauseTranslator::addAtomScan(Own<ram::Operation> op, const ast::Atom* atom,
        const ast::Clause& clause, std::size_t curLevel) const {
    const ast::Atom* head = clause.getHead();

    // add constraints
    op = addConstantConstraints(curLevel, atom->getArguments(), std::move(op));

    // add check for emptiness for an atom
    op = mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::EmptinessCheck>(getClauseAtomName(clause, atom))), std::move(op));

    // check whether all arguments are unnamed variables
    bool isAllArgsUnnamed = all_of(
            atom->getArguments(), [&](const ast::Argument* arg) { return isA<ast::UnnamedVariable>(arg); });

    // add a scan level
    if (atom->getArity() != 0 && !isAllArgsUnnamed) {
        if (head->getArity() == 0) {
            op = mk<ram::Break>(mk<ram::Negation>(mk<ram::EmptinessCheck>(getClauseAtomName(clause, head))),
                    std::move(op));
        }

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
    }

    return op;
}

Own<ram::Operation> ClauseTranslator::addRecordUnpack(
        Own<ram::Operation> op, const ast::RecordInit* rec, std::size_t curLevel) const {
    // add constant constraints
    op = addConstantConstraints(curLevel, rec->getArguments(), std::move(op));

    // add an unpack level
    const Location& loc = valueIndex->getDefinitionPoint(*rec);
    op = mk<ram::UnpackRecord>(std::move(op), curLevel, makeRamTupleElement(loc), rec->getArguments().size());
    return op;
}

Own<ram::Operation> ClauseTranslator::addAdtUnpack(
        Own<ram::Operation> op, const ast::BranchInit* adt, std::size_t curLevel) const {
    assert(!context.isADTEnum(adt) && "ADT enums should not be unpacked");

    std::vector<ast::Argument*> branchArguments;

    std::size_t branchLevel;
    // only for ADT with arity less than two (= simple)
    // add padding for branch id
    auto dummyArg = mk<ast::UnnamedVariable>();

    if (context.isADTBranchSimple(adt)) {
        // for ADT with arity < 2, we have a single level
        branchLevel = curLevel;
        branchArguments.push_back(dummyArg.get());
    } else {
        // for ADT with arity < 2, we have two levels of
        // nesting, the second one being for the arguments
        branchLevel = curLevel - 1;
    }

    for (auto* arg : adt->getArguments()) {
        branchArguments.push_back(arg);
    }

    // set branch tag constraint
    op = addEqualityCheck(std::move(op), mk<ram::TupleElement>(branchLevel, 0),
            mk<ram::SignedConstant>(context.getADTBranchId(adt)), false);

    if (context.isADTBranchSimple(adt)) {
        op = addConstantConstraints(branchLevel, branchArguments, std::move(op));
    } else {
        op = addConstantConstraints(curLevel, branchArguments, std::move(op));
        op = mk<ram::UnpackRecord>(
                std::move(op), curLevel, mk<ram::TupleElement>(branchLevel, 1), branchArguments.size());
    }

    const Location& loc = valueIndex->getDefinitionPoint(*adt);
    // add an unpack level for main record
    op = mk<ram::UnpackRecord>(std::move(op), branchLevel, makeRamTupleElement(loc), 2);

    return op;
}

Own<ram::Operation> ClauseTranslator::addVariableIntroductions(
        const ast::Clause& clause, Own<ram::Operation> op) {
    for (std::size_t p = operators.size(); p > 0; p--) {
        std::size_t i = p - 1;
        const auto* curOp = operators.at(i);
        if (const auto* atom = as<ast::Atom>(curOp)) {
            // add atom arguments through a scan
            op = addAtomScan(std::move(op), atom, clause, i);
        } else if (const auto* rec = as<ast::RecordInit>(curOp)) {
            // add record arguments through an unpack
            op = addRecordUnpack(std::move(op), rec, i);
        } else if (const auto* adt = as<ast::BranchInit>(curOp)) {
            // add adt arguments through an unpack
            op = addAdtUnpack(std::move(op), adt, i);
            if (!context.isADTBranchSimple(adt)) {
                // for non-simple ADTs (arity > 1), we introduced two
                // nesting levels
                p--;
            }
        } else {
            fatal("Unsupported AST node for creation of scan-level!");
        }
    }
    return op;
}

Own<ram::Operation> ClauseTranslator::instantiateAggregator(Own<ram::Operation> op, const ast::Clause& clause,
        const ast::Aggregator* agg, std::size_t curLevel) const {
    auto addAggEqCondition = [&](Own<ram::Condition> aggr, Own<ram::Expression> value, std::size_t pos) {
        if (isUndefValue(value.get())) return aggr;

        // TODO: float type equivalence check
        return addConjunctiveTerm(
                std::move(aggr), mk<ram::Constraint>(BinaryConstraintOp::EQ,
                                         mk<ram::TupleElement>(curLevel, pos), std::move(value)));
    };

    Own<ram::Condition> aggCond;

    // translate constraints of sub-clause
    for (const auto* lit : agg->getBodyLiterals()) {
        // literal becomes a constraint
        if (auto condition = context.translateConstraint(*valueIndex, lit)) {
            aggCond = addConjunctiveTerm(std::move(aggCond), std::move(condition));
        }
    }

    // translate arguments of atom to conditions
    const auto& aggBodyAtoms =
            filter(agg->getBodyLiterals(), [&](const ast::Literal* lit) { return isA<ast::Atom>(lit); });
    assert(aggBodyAtoms.size() == 1 && "exactly one atom should exist per aggregator body");
    const auto* aggAtom = static_cast<const ast::Atom*>(aggBodyAtoms.at(0));

    const auto& aggAtomArgs = aggAtom->getArguments();
    for (std::size_t i = 0; i < aggAtomArgs.size(); i++) {
        const auto* arg = aggAtomArgs.at(i);

        // variable bindings are issued differently since we don't want self
        // referential variable bindings
        if (auto* var = as<ast::Variable>(arg)) {
            for (auto&& loc : valueIndex->getVariableReferences(var->getName())) {
                if (curLevel != loc.identifier || i != loc.element) {
                    aggCond = addAggEqCondition(std::move(aggCond), makeRamTupleElement(loc), i);
                    break;
                }
            }
        } else {
            assert(arg != nullptr && "aggregator argument cannot be nullptr");
            auto value = context.translateValue(*valueIndex, arg);
            aggCond = addAggEqCondition(std::move(aggCond), std::move(value), i);
        }
    }

    // translate aggregate expression
    const auto* aggExpr = agg->getTargetExpression();
    auto expr = aggExpr ? context.translateValue(*valueIndex, aggExpr) : nullptr;

    // add Ram-Aggregation layer
    return mk<ram::Aggregate>(std::move(op), context.getOverloadedAggregatorOperator(*agg),
            getClauseAtomName(clause, aggAtom), expr ? std::move(expr) : mk<ram::UndefValue>(),
            aggCond ? std::move(aggCond) : mk<ram::True>(), curLevel);
}

Own<ram::Operation> ClauseTranslator::instantiateMultiResultFunctor(
        Own<ram::Operation> op, const ast::IntrinsicFunctor& inf, std::size_t curLevel) const {
    VecOwn<ram::Expression> args;
    for (auto&& x : inf.getArguments()) {
        args.push_back(context.translateValue(*valueIndex, x));
    }

    auto func_op = [&]() -> ram::NestedIntrinsicOp {
        switch (context.getOverloadedFunctorOp(inf)) {
            case FunctorOp::RANGE: return ram::NestedIntrinsicOp::RANGE;
            case FunctorOp::URANGE: return ram::NestedIntrinsicOp::URANGE;
            case FunctorOp::FRANGE: return ram::NestedIntrinsicOp::FRANGE;

            default: fatal("missing case handler or bad code-gen");
        }
    };

    return mk<ram::NestedIntrinsicOperator>(func_op(), std::move(args), std::move(op), curLevel);
}

Own<ram::Operation> ClauseTranslator::addGeneratorLevels(
        Own<ram::Operation> op, const ast::Clause& clause) const {
    std::size_t curLevel = operators.size() + generators.size() - 1;
    for (const auto* generator : reverse(generators)) {
        if (auto agg = as<ast::Aggregator>(generator)) {
            op = instantiateAggregator(std::move(op), clause, agg, curLevel);
        } else if (const auto* inf = as<ast::IntrinsicFunctor>(generator)) {
            op = instantiateMultiResultFunctor(std::move(op), *inf, curLevel);
        } else {
            assert(false && "unhandled generator");
        }
        curLevel--;
    }
    return op;
}

Own<ram::Operation> ClauseTranslator::addDistinct(
        Own<ram::Operation> op, const ast::Atom* atom1, const ast::Atom* atom2) const {
    std::size_t arity = atom1->getArity();

    VecOwn<ram::Condition> conditions;
    auto args1 = atom1->getArguments();
    auto args2 = atom2->getArguments();
    for (std::size_t i = 0; i < arity; i++) {
        Own<ram::Expression> a1 = context.translateValue(*valueIndex, args1[i]);
        Own<ram::Expression> a2 = context.translateValue(*valueIndex, args2[i]);
        if (*a1 != *a2) {
            conditions.push_back(mk<ram::Constraint>(BinaryConstraintOp::EQ, std::move(a1), std::move(a2)));
        }
    }
    return mk<ram::Filter>(mk<ram::Negation>(toCondition(conditions)), std::move(op));
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
    for (std::size_t i = 0; i < arity; i++) {
        values.push_back(context.translateValue(*valueIndex, args[i]));
    }

    return mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::ExistenceCheck>(name, std::move(values))), std::move(op));
}

Own<ram::Operation> ClauseTranslator::addNegatedAtom(
        Own<ram::Operation> op, const ast::Clause& clause, const ast::Atom* atom) const {
    std::size_t arity = atom->getArity();
    std::string name = getConcreteRelationName(atom->getQualifiedName());

    if (arity == 0) {
        // for a nullary, negation is a simple emptiness check
        return mk<ram::Filter>(mk<ram::EmptinessCheck>(name), std::move(op));
    }

    // else, we construct the atom and create a negation
    VecOwn<ram::Expression> values;
    auto args = atom->getArguments();
    for (std::size_t i = 0; i < arity; i++) {
        values.push_back(context.translateValue(*valueIndex, args[i]));
    }
    
    const ast::Atom* head = clause.getHead();
    const auto* relation = context.getProgram()->getRelation(*head);
    if (
        relation->getRepresentation() == RelationRepresentation::BTREE_MIN || 
        relation->getRepresentation() == RelationRepresentation::BTREE_MAX
    ) {
        return mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::AggregateExistenceCheck>(name, std::move(values))), std::move(op));
    } else {
        return mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::ExistenceCheck>(name, std::move(values))), std::move(op));
    }
}

Own<ram::Operation> ClauseTranslator::addBodyLiteralConstraints(
        const ast::Clause& clause, Own<ram::Operation> op) const {
    for (const auto* lit : clause.getBodyLiterals()) {
        // constraints become literals
        if (auto condition = context.translateConstraint(*valueIndex, lit)) {
            op = mk<ram::Filter>(std::move(condition), std::move(op));
        }
    }

    if (isA<ast::SubsumptiveClause>(clause)) {
        if (mode == SubsumeRejectNewNew || mode == SubsumeDeleteCurrentCurrent) {
            // find the dominated / dominating heads
            const auto& body = clause.getBodyLiterals();
            auto dominatedHeadAtom = dynamic_cast<const ast::Atom*>(body[0]);
            auto dominatingHeadAtom = dynamic_cast<const ast::Atom*>(body[1]);
            op = addDistinct(std::move(op), dominatedHeadAtom, dominatingHeadAtom);
        }
        return op;
    }

    if (isRecursive()) {
        if (clause.getHead()->getArity() > 0) {
            // also negate the head
            op = addNegatedAtom(std::move(op), clause, clause.getHead());
        }

        // also add in prev stuff
        for (std::size_t i = version + 1; i < sccAtoms.size(); i++) {
            op = addNegatedDeltaAtom(std::move(op), sccAtoms.at(i));
        }
    }

    return op;
}

Own<ram::Condition> ClauseTranslator::createCondition(const ast::Clause& clause) const {
    const auto head = clause.getHead();

    // add stopping criteria for nullary relations
    // (if it contains already the null tuple, don't re-compute)
    if (isRecursive() && head->getArity() == 0) {
        return mk<ram::EmptinessCheck>(getConcreteRelationName(head->getQualifiedName()));
    }
    return nullptr;
}

Own<ram::Expression> ClauseTranslator::translateConstant(const ast::Constant& constant) const {
    if (auto strConstant = as<ast::StringConstant>(constant)) {
        return mk<ram::StringConstant>(strConstant->getConstant());
    } else if (isA<ast::NilConstant>(&constant)) {
        return mk<ram::SignedConstant>(0);
    } else if (auto* numConstant = as<ast::NumericConstant>(constant)) {
        switch (context.getInferredNumericConstantType(*numConstant)) {
            case ast::NumericConstant::Type::Int:
                return mk<ram::SignedConstant>(RamSignedFromString(numConstant->getConstant(), nullptr, 0));
            case ast::NumericConstant::Type::Uint:
                return mk<ram::UnsignedConstant>(
                        RamUnsignedFromString(numConstant->getConstant(), nullptr, 0));
            case ast::NumericConstant::Type::Float:
                return mk<ram::FloatConstant>(RamFloatFromString(numConstant->getConstant()));
        }
    }
    fatal("unaccounted-for constant");
}

Own<ram::Operation> ClauseTranslator::addEqualityCheck(
        Own<ram::Operation> op, Own<ram::Expression> lhs, Own<ram::Expression> rhs, bool isFloat) const {
    auto eqOp = isFloat ? BinaryConstraintOp::FEQ : BinaryConstraintOp::EQ;
    auto eqConstraint = mk<ram::Constraint>(eqOp, std::move(lhs), std::move(rhs));
    return mk<ram::Filter>(std::move(eqConstraint), std::move(op));
}

Own<ram::Operation> ClauseTranslator::addConstantConstraints(
        std::size_t curLevel, const std::vector<ast::Argument*>& arguments, Own<ram::Operation> op) const {
    for (std::size_t i = 0; i < arguments.size(); i++) {
        const auto* argument = arguments.at(i);
        if (const auto* numericConstant = as<ast::NumericConstant>(argument)) {
            bool isFloat = context.getInferredNumericConstantType(*numericConstant) ==
                           ast::NumericConstant::Type::Float;
            auto lhs = mk<ram::TupleElement>(curLevel, i);
            auto rhs = translateConstant(*numericConstant);
            op = addEqualityCheck(std::move(op), std::move(lhs), std::move(rhs), isFloat);
        } else if (const auto* constant = as<ast::Constant>(argument)) {
            auto lhs = mk<ram::TupleElement>(curLevel, i);
            auto rhs = translateConstant(*constant);
            op = addEqualityCheck(std::move(op), std::move(lhs), std::move(rhs), false);
        } else if (const auto* adt = as<ast::BranchInit>(argument)) {
            if (context.isADTEnum(adt)) {
                auto lhs = mk<ram::TupleElement>(curLevel, i);
                auto rhs = mk<ram::SignedConstant>(context.getADTBranchId(adt));
                op = addEqualityCheck(std::move(op), std::move(lhs), std::move(rhs), false);
            }
        }
    }

    return op;
}

Own<ram::Condition> ClauseTranslator::getFunctionalDependencies(const ast::Clause& clause) const {
    const auto* head = clause.getHead();
    const auto* relation = context.getProgram()->getRelation(clause);
    if (relation->getFunctionalDependencies().empty()) {
        return nullptr;
    }

    std::string headRelationName = getClauseAtomName(clause, head);
    const auto& attributes = relation->getAttributes();
    const auto& headArgs = head->getArguments();

    // Impose the functional dependencies of the relation on each INSERT
    VecOwn<ram::Condition> dependencies;
    std::vector<const ast::FunctionalConstraint*> addedConstraints;
    for (const auto* fd : relation->getFunctionalDependencies()) {
        // Skip if already seen
        bool alreadySeen = false;
        for (const auto* other : addedConstraints) {
            if (other->equivalentConstraint(*fd)) {
                alreadySeen = true;
                break;
            }
        }
        if (alreadySeen) {
            continue;
        }

        // Remove redundant attributes within the same key
        addedConstraints.push_back(fd);
        std::set<std::string> keys;
        for (auto key : fd->getKeys()) {
            keys.insert(key->getName());
        }

        // Grab the necessary head arguments
        VecOwn<ram::Expression> vals;
        VecOwn<ram::Expression> valsCopy;
        for (std::size_t i = 0; i < attributes.size(); ++i) {
            const auto attribute = attributes[i];
            if (contains(keys, attribute->getName())) {
                // If this particular source argument matches the head argument, insert it.
                vals.push_back(context.translateValue(*valueIndex, headArgs.at(i)));
                valsCopy.push_back(context.translateValue(*valueIndex, headArgs.at(i)));
            } else {
                // Otherwise insert ⊥
                vals.push_back(mk<ram::UndefValue>());
                valsCopy.push_back(mk<ram::UndefValue>());
            }
        }

        if (isRecursive()) {
            // If we are in a recursive clause, need to guard both new and original relation.
            dependencies.push_back(
                    mk<ram::Negation>(mk<ram::ExistenceCheck>(headRelationName, std::move(vals))));
            dependencies.push_back(mk<ram::Negation>(mk<ram::ExistenceCheck>(
                    getConcreteRelationName(relation->getQualifiedName()), std::move(valsCopy))));
        } else {
            dependencies.push_back(
                    mk<ram::Negation>(mk<ram::ExistenceCheck>(headRelationName, std::move(vals))));
        }
    }

    return ram::toCondition(dependencies);
}

std::vector<ast::Atom*> ClauseTranslator::getAtomOrdering(const ast::Clause& clause) const {
    auto atoms = ast::getBodyLiterals<ast::Atom>(clause);

    // stick to the plan if we have one set
    auto* plan = clause.getExecutionPlan();
    if (plan != nullptr) {
        auto orders = plan->getOrders();
        if (contains(orders, version)) {
            // get the imposed order, and change it to start at zero
            const auto& order = orders.at(version);
            std::vector<std::size_t> newOrder(order->getOrder().size());
            std::transform(order->getOrder().begin(), order->getOrder().end(), newOrder.begin(),
                    [](std::size_t i) -> std::size_t { return i - 1; });
            return reorderAtoms(atoms, newOrder);
        }
    }

    auto newOrder = context.getSipsMetric()->getReordering(&clause, version, mode);
    return reorderAtoms(atoms, newOrder);
}

std::size_t ClauseTranslator::addOperatorLevel(const ast::Node* node) {
    std::size_t nodeLevel = operators.size() + generators.size();
    operators.push_back(node);
    return nodeLevel;
}

std::size_t ClauseTranslator::addGeneratorLevel(const ast::Argument* arg) {
    std::size_t generatorLevel = operators.size() + generators.size();
    generators.push_back(arg);
    return generatorLevel;
}

void ClauseTranslator::indexNodeArguments(
        std::size_t nodeLevel, const std::vector<ast::Argument*>& nodeArgs) {
    for (std::size_t i = 0; i < nodeArgs.size(); i++) {
        const auto& arg = nodeArgs.at(i);

        // check for variable references
        if (const auto* var = as<ast::Variable>(arg)) {
            valueIndex->addVarReference(var->getName(), nodeLevel, i);
        }

        // check for nested records
        if (const auto* rec = as<ast::RecordInit>(arg)) {
            valueIndex->setRecordDefinition(*rec, nodeLevel, i);

            // introduce new nesting level for unpack
            auto unpackLevel = addOperatorLevel(rec);
            indexNodeArguments(unpackLevel, rec->getArguments());
        }

        // check for nested ADT branches
        if (const auto* adt = as<ast::BranchInit>(arg)) {
            if (!context.isADTEnum(adt)) {
                valueIndex->setAdtDefinition(*adt, nodeLevel, i);
                auto unpackLevel = addOperatorLevel(adt);

                if (context.isADTBranchSimple(adt)) {
                    std::vector<ast::Argument*> arguments;
                    auto dummyArg = mk<ast::UnnamedVariable>();
                    arguments.push_back(dummyArg.get());
                    for (auto* arg : adt->getArguments()) {
                        arguments.push_back(arg);
                    }
                    indexNodeArguments(unpackLevel, arguments);
                } else {
                    auto argumentUnpackLevel = addOperatorLevel(adt);
                    indexNodeArguments(argumentUnpackLevel, adt->getArguments());
                }
            }
        }
    }
}

void ClauseTranslator::indexGenerator(const ast::Argument& arg) {
    std::size_t aggLoc = addGeneratorLevel(&arg);
    valueIndex->setGeneratorLoc(arg, Location({aggLoc, 0}));
}

void ClauseTranslator::indexAtoms(const ast::Clause& clause) {
    for (const auto* atom : getAtomOrdering(clause)) {
        // give the atom the current level
        std::size_t scanLevel = addOperatorLevel(atom);
        indexNodeArguments(scanLevel, atom->getArguments());
    }
}

void ClauseTranslator::indexAggregatorBody(const ast::Aggregator& agg) {
    auto aggLoc = valueIndex->getGeneratorLoc(agg);

    // Get the single body atom inside the aggregator
    const auto& aggBodyAtoms =
            filter(agg.getBodyLiterals(), [&](const ast::Literal* lit) { return isA<ast::Atom>(lit); });
    assert(aggBodyAtoms.size() == 1 && "exactly one atom should exist per aggregator body");
    const auto* aggAtom = static_cast<const ast::Atom*>(aggBodyAtoms.at(0));

    // Add the variable references inside this atom
    const auto& aggAtomArgs = aggAtom->getArguments();
    for (std::size_t i = 0; i < aggAtomArgs.size(); i++) {
        const auto* arg = aggAtomArgs.at(i);
        if (const auto* var = as<ast::Variable>(arg)) {
            valueIndex->addVarReference(var->getName(), aggLoc.identifier, i);
        }
    }
}

void ClauseTranslator::indexAggregators(const ast::Clause& clause) {
    // Add each aggregator as an internal generator
    visit(clause, [&](const ast::Aggregator& agg) { indexGenerator(agg); });

    // Index aggregator bodies
    visit(clause, [&](const ast::Aggregator& agg) { indexAggregatorBody(agg); });

    // Add aggregator value introductions
    visit(clause, [&](const ast::BinaryConstraint& bc) {
        if (!isEqConstraint(bc.getBaseOperator())) return;
        const auto* lhs = as<ast::Variable>(bc.getLHS());
        const auto* rhs = as<ast::Aggregator>(bc.getRHS());
        if (lhs == nullptr || rhs == nullptr) return;
        valueIndex->addVarReference(lhs->getName(), valueIndex->getGeneratorLoc(*rhs));
    });
}

void ClauseTranslator::indexMultiResultFunctors(const ast::Clause& clause) {
    // Add each multi-result functor as an internal generator
    visit(clause, [&](const ast::IntrinsicFunctor& func) {
        if (ast::analysis::FunctorAnalysis::isMultiResult(func)) {
            indexGenerator(func);
        }
    });

    // Add multi-result functor value introductions
    visit(clause, [&](const ast::BinaryConstraint& bc) {
        if (!isEqConstraint(bc.getBaseOperator())) return;
        const auto* lhs = as<ast::Variable>(bc.getLHS());
        const auto* rhs = as<ast::IntrinsicFunctor>(bc.getRHS());
        if (lhs == nullptr || rhs == nullptr) return;
        if (!ast::analysis::FunctorAnalysis::isMultiResult(*rhs)) return;
        valueIndex->addVarReference(lhs->getName(), valueIndex->getGeneratorLoc(*rhs));
    });
}

void ClauseTranslator::indexClause(const ast::Clause& clause) {
    indexAtoms(clause);
    indexAggregators(clause);
    indexMultiResultFunctors(clause);
}

}  // namespace souffle::ast2ram::seminaive
