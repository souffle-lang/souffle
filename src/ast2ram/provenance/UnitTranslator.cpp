/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnitTranslator.h
 *
 ***********************************************************************/

#include "ast2ram/provenance/UnitTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/provenance/SubproofGenerator.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/AbstractOperator.h"
#include "ram/Call.h"
#include "ram/DebugInfo.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/MergeExtend.h"
#include "ram/Negation.h"
#include "ram/Query.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/StringConstant.h"
#include "ram/SubroutineArgument.h"
#include "ram/SubroutineReturn.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "souffle/SymbolTable.h"
#include "souffle/utility/StringUtil.h"
#include <sstream>

namespace souffle::ast2ram::provenance {

Own<ram::Sequence> UnitTranslator::generateProgram(const ast::TranslationUnit& translationUnit) {
    glb = &translationUnit.global();

    // Do the regular translation
    auto ramProgram = seminaive::UnitTranslator::generateProgram(translationUnit);

    // Create info clauses
    auto infoClauses = generateInfoClauses(context->getProgram());

    // Combine into a single ram program
    ramProgram = mk<ram::Sequence>(std::move(ramProgram), std::move(infoClauses));

    // Add subroutines for each clause
    addProvenanceClauseSubroutines(context->getProgram());

    return ramProgram;
}

Own<ram::Relation> UnitTranslator::createRamRelation(
        const ast::Relation* baseRelation, std::string ramRelationName) const {
    auto relation = seminaive::UnitTranslator::createRamRelation(baseRelation, ramRelationName);

    std::size_t arity = relation->getArity();
    std::size_t auxiliaryArity = relation->getAuxiliaryArity();
    std::vector<std::string> attributeNames = relation->getAttributeNames();
    std::vector<std::string> attributeTypeQualifiers = relation->getAttributeTypes();

    // Add in provenance information
    attributeNames.push_back("@rule_number");
    attributeTypeQualifiers.push_back("i:number");

    attributeNames.push_back("@level_number");
    attributeTypeQualifiers.push_back("i:number");

    return mk<ram::Relation>(ramRelationName, arity + 2, auxiliaryArity + 2, attributeNames,
            attributeTypeQualifiers, relation->getRepresentation());
}

std::string UnitTranslator::getInfoRelationName(const ast::Clause* clause) const {
    std::size_t clauseID = context->getClauseNum(clause);
    auto infoRelQualifiedName = clause->getHead()->getQualifiedName();
    infoRelQualifiedName.append("@info");
    infoRelQualifiedName.append(toString(clauseID));
    return getConcreteRelationName(infoRelQualifiedName);
}

VecOwn<ram::Relation> UnitTranslator::createRamRelations(const std::vector<std::size_t>& sccOrdering) const {
    // Regular relations
    auto ramRelations = seminaive::UnitTranslator::createRamRelations(sccOrdering);

    // Info relations
    for (const auto* clause : context->getProgram()->getClauses()) {
        if (isFact(*clause)) {
            continue;
        }

        std::vector<std::string> attributeNames;
        std::vector<std::string> attributeTypeQualifiers;

        // (1) Clause ID
        attributeNames.push_back("clause_num");
        attributeTypeQualifiers.push_back("i:number");

        // (2) Head variable string
        attributeNames.push_back("head_vars");
        attributeTypeQualifiers.push_back("s:symbol");

        // (3) For all atoms + negs + bcs: rel_<i>:symbol
        auto bodyLiterals = clause->getBodyLiterals();
        for (std::size_t i = 0; i < bodyLiterals.size(); i++) {
            const auto* literal = bodyLiterals.at(i);
            if (isA<ast::Atom>(literal) || isA<ast::Negation>(literal) ||
                    isA<ast::BinaryConstraint>(literal)) {
                attributeNames.push_back("rel_" + std::to_string(i));
                attributeTypeQualifiers.push_back("s:symbol");
            }
        }

        // (4) Clause representation
        attributeNames.push_back("clause_repr");
        attributeTypeQualifiers.push_back("s:symbol");

        // Create the info relation
        ramRelations.push_back(mk<ram::Relation>(getInfoRelationName(clause), attributeNames.size(), 0,
                attributeNames, attributeTypeQualifiers, RelationRepresentation::INFO));
    }

    return ramRelations;
}

void UnitTranslator::addAuxiliaryArity(
        const ast::Relation* /* relation */, std::map<std::string, std::string>& directives) const {
    directives.insert(std::make_pair("auxArity", "2"));
}

Own<ram::Statement> UnitTranslator::generateClearExpiredRelations(
        const ast::RelationSet& /* expiredRelations */) const {
    // Relations should be preserved if provenance is enabled
    return mk<ram::Sequence>();
}

void UnitTranslator::addProvenanceClauseSubroutines(const ast::Program* program) {
    visit(*program, [&](const ast::Clause& clause) {
        // Skip facts
        if (isFact(clause)) {
            return;
        }

        std::string relName = toString(clause.getHead()->getQualifiedName());

        // Positive subproof
        std::string subroutineLabel =
                relName + "_" + std::to_string(context->getClauseNum(&clause)) + "_subproof";
        addRamSubroutine(subroutineLabel, makeSubproofSubroutine(clause));

        // Negative subproof
        std::string negationSubroutineLabel =
                relName + "_" + std::to_string(context->getClauseNum(&clause)) + "_negation_subproof";
        addRamSubroutine(negationSubroutineLabel, makeNegationSubproofSubroutine(clause));
    });
}

Own<ram::Statement> UnitTranslator::generateMergeRelations(
        const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const {
    VecOwn<ram::Expression> values;

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity() + 2; i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
    }

    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(insertion)));
    if (rel->getRepresentation() == RelationRepresentation::EQREL) {
        return mk<ram::Sequence>(mk<ram::MergeExtend>(destRelation, srcRelation), std::move(stmt));
    }
    return stmt;
}

Own<ram::Sequence> UnitTranslator::generateInfoClauses(const ast::Program* program) {
    VecOwn<ram::Statement> infoClauseCalls;

    std::size_t stratumCount = context->getNumberOfSCCs();
    for (const auto* clause : program->getClauses()) {
        if (isFact(*clause)) {
            continue;
        }
        std::size_t clauseID = context->getClauseNum(clause);

        // Argument info generator
        int functorNumber = 0;
        int aggregateNumber = 0;
        int typecastNumber = 0;
        int termNumber = 0;
        auto getArgInfo = [&](const ast::Argument* arg) -> std::string {
            if (auto* var = as<ast::Variable>(arg)) {
                return toString(*var);
            }
            if (auto* constant = as<ast::Constant>(arg)) {
                return toString(*constant);
            }
            if (isA<ast::UnnamedVariable>(arg)) {
                return "_";
            }
            if (isA<ast::Functor>(arg)) {
                return tfm::format("functor_%d", functorNumber++);
            }
            if (isA<ast::Aggregator>(arg)) {
                return tfm::format("agg_%d", aggregateNumber++);
            }
            if (isA<ast::TypeCast>(arg)) {
                return tfm::format("typecast_%d", typecastNumber++);
            }
            if (isA<ast::Term>(arg)) {
                return tfm::format("term_%d", termNumber++);
            }
            if (isA<ast::Counter>(arg)) {
                fatal("Unhandled argument type: Counter");
            }
            fatal("Unhandled unknown argument type");
        };

        // Generate clause head arguments
        VecOwn<ram::Expression> factArguments;

        // (1) Clause ID
        factArguments.push_back(mk<ram::SignedConstant>(clauseID));

        // (2) Head variables
        std::vector<std::string> headVariables;
        for (const auto* arg : clause->getHead()->getArguments()) {
            headVariables.push_back(getArgInfo(arg));
        }
        std::stringstream headVariableInfo;
        headVariableInfo << join(headVariables, ",");
        factArguments.push_back(mk<ram::StringConstant>(headVariableInfo.str()));

        // (3) For all atoms || negs:
        //      - atoms: relName,{atom arg info}
        //      - negs: !relName
        for (const auto* literal : clause->getBodyLiterals()) {
            if (const auto* atom = as<ast::Atom>(literal)) {
                std::string atomDescription = toString(atom->getQualifiedName());
                for (const auto* arg : atom->getArguments()) {
                    atomDescription.append("," + getArgInfo(arg));
                }
                factArguments.push_back(mk<ram::StringConstant>(atomDescription));
            } else if (const auto* neg = as<ast::Negation>(literal)) {
                const auto* atom = neg->getAtom();
                std::string relName = toString(atom->getQualifiedName());
                factArguments.push_back(mk<ram::StringConstant>("!" + relName));
            }
        }

        // (4) For all bcs:
        //      - symbol,lhs arg info,rhs arg info
        for (const auto* binaryConstraint : ast::getBodyLiterals<ast::BinaryConstraint>(*clause)) {
            std::stringstream constraintDescription;
            constraintDescription << toBinaryConstraintSymbol(binaryConstraint->getBaseOperator());
            constraintDescription << "," << getArgInfo(binaryConstraint->getLHS());
            constraintDescription << "," << getArgInfo(binaryConstraint->getRHS());
            factArguments.push_back(mk<ram::StringConstant>(constraintDescription.str()));
        }

        // (5) The actual clause
        factArguments.push_back(mk<ram::StringConstant>(toString(*clause)));

        /* -- Finalising -- */
        // Push in the final clause
        std::string infoRelName = getInfoRelationName(clause);
        auto factInsertion = mk<ram::Insert>(infoRelName, std::move(factArguments));
        auto infoClause = mk<ram::Statement, ram::Query>(std::move(factInsertion));

        // Add logging
        if (glb->config().has("profile")) {
            std::stringstream clauseText;
            clauseText << "@info.clause[" << stringify(toString(*clause)) << "]";
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(infoRelName, clause->getSrcLoc(), clauseText.str());
            infoClause = mk<ram::LogRelationTimer>(std::move(infoClause), logTimerStatement, infoRelName);
        }

        // Add debug info
        std::ostringstream ds;
        ds << "@info.clause[";
        clause->printForDebugInfo(ds);
        ds << "]"
           << "\nin file ";
        ds << clause->getSrcLoc();
        infoClause = mk<ram::DebugInfo>(std::move(infoClause), ds.str());

        // Add the subroutine to the program
        std::string stratumID = toString(stratumCount++);
        addRamSubroutine(stratumID, std::move(infoClause));

        // Push up the subroutine call
        infoClauseCalls.push_back(mk<ram::Call>("stratum_" + stratumID));
    }

    return mk<ram::Sequence>(std::move(infoClauseCalls));
}

Own<ram::Statement> UnitTranslator::makeSubproofSubroutine(const ast::Clause& clause) {
    return SubproofGenerator(*context).translateNonRecursiveClause(clause);
}

Own<ram::ExistenceCheck> UnitTranslator::makeRamAtomExistenceCheck(const ast::Atom* atom,
        const std::map<std::size_t, std::string>& idToVarName, ValueIndex& valueIndex) const {
    auto relName = getConcreteRelationName(atom->getQualifiedName());

    // Construct a query
    VecOwn<ram::Expression> query;

    // Add each value (subroutine argument) to the search query
    for (const auto* arg : atom->getArguments()) {
        auto translatedValue = context->translateValue(valueIndex, arg);
        transformVariablesToSubroutineArgs(translatedValue.get(), idToVarName);
        query.push_back(std::move(translatedValue));
    }

    // Fill up query with nullptrs for the provenance columns
    query.push_back(mk<ram::UndefValue>());
    query.push_back(mk<ram::UndefValue>());

    // Create existence checks to check if the tuple exists or not
    return mk<ram::ExistenceCheck>(relName, std::move(query));
}

Own<ram::SubroutineReturn> UnitTranslator::makeRamReturnTrue() const {
    VecOwn<ram::Expression> returnTrue;
    returnTrue.push_back(mk<ram::SignedConstant>(1));
    return mk<ram::SubroutineReturn>(std::move(returnTrue));
}

Own<ram::SubroutineReturn> UnitTranslator::makeRamReturnFalse() const {
    VecOwn<ram::Expression> returnFalse;
    returnFalse.push_back(mk<ram::SignedConstant>(0));
    return mk<ram::SubroutineReturn>(std::move(returnFalse));
}

void UnitTranslator::transformVariablesToSubroutineArgs(
        ram::Node* node, const std::map<std::size_t, std::string>& idToVarName) const {
    // A mapper to replace variables with subroutine arguments
    struct VariablesToArguments : public ram::NodeMapper {
        const std::map<std::size_t, std::string>& idToVarName;

        VariablesToArguments(const std::map<std::size_t, std::string>& idToVarName)
                : idToVarName(idToVarName) {}

        Own<ram::Node> operator()(Own<ram::Node> node) const override {
            if (const auto* tuple = as<ram::TupleElement>(node.get())) {
                const auto& varName = idToVarName.at(tuple->getTupleId());
                // TODO (azreika): shouldn't rely on name for singleton variables; do an index check
                if (isPrefix("+underscore", varName)) {
                    return mk<ram::UndefValue>();
                }
                return mk<ram::SubroutineArgument>(tuple->getTupleId());
            }

            // Apply recursive
            node->apply(*this);
            return node;
        }
    };

    VariablesToArguments varsToArgs(idToVarName);
    node->apply(varsToArgs);
}

Own<ram::Sequence> UnitTranslator::makeIfStatement(
        Own<ram::Condition> condition, Own<ram::Operation> trueOp, Own<ram::Operation> falseOp) const {
    auto negatedCondition = mk<ram::Negation>(clone(condition));

    auto trueBranch = mk<ram::Query>(mk<ram::Filter>(std::move(condition), std::move(trueOp)));
    auto falseBranch = mk<ram::Query>(mk<ram::Filter>(std::move(negatedCondition), std::move(falseOp)));

    return mk<ram::Sequence>(std::move(trueBranch), std::move(falseBranch));
}

Own<ram::Statement> UnitTranslator::makeNegationSubproofSubroutine(const ast::Clause& clause) {
    // TODO (taipan-snake): Currently we only deal with atoms (no constraints or negations or aggregates
    // or anything else...)

    // The resulting subroutine looks something like this:
    // IF (arg(0), arg(1), _, _) IN rel_1:
    //   return 1
    // IF (arg(0), arg(1), _ ,_) NOT IN rel_1:
    //   return 0
    // ...

    // Order the literals s.t. constraints are last
    std::vector<const ast::Literal*> lits;
    for (const auto* bodyLit : clause.getBodyLiterals()) {
        if (!isA<ast::Constraint>(bodyLit)) {
            lits.push_back(bodyLit);
        }
    }
    for (const auto* bodyLit : ast::getBodyLiterals<ast::Constraint>(clause)) {
        lits.push_back(bodyLit);
    }

    // Keep track of references in a dummy index
    std::size_t count = 0;
    std::map<std::size_t, std::string> idToVarName;
    auto dummyValueIndex = mk<ValueIndex>();
    visit(clause, [&](const ast::Variable& var) {
        if (dummyValueIndex->isDefined(var.getName()) || isPrefix("+underscore", var.getName())) {
            return;
        }
        idToVarName[count] = var.getName();
        dummyValueIndex->addVarReference(var.getName(), count++, 0);
    });

    visit(clause, [&](const ast::Variable& var) {
        // TODO (azreika): index aggregators too, then override the value translator to treat them as
        // variables

        // Index all unnamed variables
        // TODO (azreika): maybe don't unname variables in provenance
        if (isPrefix("+underscore", var.getName())) {
            idToVarName[count] = var.getName();
            dummyValueIndex->addVarReference(var.getName(), count++, 0);
        }
    });

    // Create the search sequence
    VecOwn<ram::Statement> searchSequence;
    for (const auto* lit : lits) {
        if (const auto* atom = as<ast::Atom>(lit)) {
            auto existenceCheck = makeRamAtomExistenceCheck(atom, idToVarName, *dummyValueIndex);
            transformVariablesToSubroutineArgs(existenceCheck.get(), idToVarName);
            auto ifStatement =
                    makeIfStatement(std::move(existenceCheck), makeRamReturnTrue(), makeRamReturnFalse());
            appendStmt(searchSequence, std::move(ifStatement));
        } else if (const auto* neg = as<ast::Negation>(lit)) {
            auto existenceCheck = makeRamAtomExistenceCheck(neg->getAtom(), idToVarName, *dummyValueIndex);
            transformVariablesToSubroutineArgs(existenceCheck.get(), idToVarName);
            auto ifStatement =
                    makeIfStatement(std::move(existenceCheck), makeRamReturnFalse(), makeRamReturnTrue());
            appendStmt(searchSequence, std::move(ifStatement));
        } else if (const auto* con = as<ast::Constraint>(lit)) {
            bool hasComplexity = false;
            visit(clause, [&](const ast::Aggregator&) { hasComplexity = true; });
            visit(clause, [&](const ast::IntrinsicFunctor& f) {
                if (f.getBaseFunctionOp() == "range") hasComplexity = true;
            });
            if (!hasComplexity) {
                auto condition = context->translateConstraint(*dummyValueIndex, con);
                transformVariablesToSubroutineArgs(condition.get(), idToVarName);
                auto ifStatement =
                        makeIfStatement(std::move(condition), makeRamReturnTrue(), makeRamReturnFalse());
                appendStmt(searchSequence, std::move(ifStatement));
            }
        }
    }

    return mk<ram::Sequence>(std::move(searchSequence));
}

}  // namespace souffle::ast2ram::provenance
