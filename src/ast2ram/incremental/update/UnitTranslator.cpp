/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnitTranslator.cpp
 *
 ***********************************************************************/

#include "ast2ram/incremental/update/UnitTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/BinaryConstraint.h"
#include "ast/Clause.h"
#include "ast/Constraint.h"
#include "ast/analysis/TopologicallySortedSCCGraph.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/DebugInfo.h"
#include "ram/ExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/Insert.h"
#include "ram/LogSize.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogTimer.h"
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
#include "ram/Swap.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "souffle/SymbolTable.h"
#include "souffle/utility/StringUtil.h"
#include <sstream>

namespace souffle::ast2ram::incremental::update {

void UnitTranslator::addRamSubroutine(std::string subroutineID, Own<ram::Statement> subroutine) {
    assert(!contains(updateRamSubroutines, subroutineID) && "subroutine ID should not already exist");
    updateRamSubroutines[subroutineID] = std::move(subroutine);
}

Own<ram::Sequence> UnitTranslator::generateProgram(const ast::TranslationUnit& translationUnit) {
    // Do the regular translation
    // auto ramProgram = seminaive::UnitTranslator::generateProgram(translationUnit);

    // auto ramProgram = mk<ram::Sequence>();
    // return ramProgram;

    // Generate context here
    context = mk<TranslatorContext>(translationUnit, true);

    const auto& sccOrdering =
            translationUnit.getAnalysis<ast::analysis::TopologicallySortedSCCGraphAnalysis>().order();

    std::cout << "scc ordering size: " << sccOrdering.size() << std::endl;

    // Create subroutines for each SCC according to topological order
    for (std::size_t i = 0; i < sccOrdering.size(); i++) {
        // Generate the main stratum code
        auto stratum = generateStratum(sccOrdering.at(i));

        // Clear expired relations
        const auto& expiredRelations = context->getExpiredRelations(i);
        stratum = mk<ram::Sequence>(std::move(stratum), generateClearExpiredRelations(expiredRelations));

        // Add the subroutine
        std::string stratumID = "update_stratum_" + toString(i);
        addRamSubroutine(stratumID, std::move(stratum));
    }

    // Invoke all strata
    VecOwn<ram::Statement> res;
    for (std::size_t i = 0; i < sccOrdering.size(); i++) {
        appendStmt(res, mk<ram::Call>("update_stratum_" + toString(i)));
    }

    // Add main timer if profiling
    if (!res.empty() && Global::config().has("profile")) {
        auto newStmt = mk<ram::LogTimer>(mk<ram::Sequence>(std::move(res)), LogStatement::runtime());
        res.clear();
        appendStmt(res, std::move(newStmt));
    }

    // Program translated!
    return mk<ram::Sequence>(std::move(res));
}

Own<ram::Statement> UnitTranslator::generateNonRecursiveRelation(const ast::Relation& rel) const {
    auto addProfiling = [](const ast::Relation& rel, const ast::Clause* clause, Own<ram::Statement> stmt) -> Own<ram::Statement> {
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const auto& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement = LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            return mk<ram::LogRelationTimer>(std::move(stmt), logTimerStatement,
                    getConcreteRelationName(rel.getQualifiedName()));
        }
        return stmt;
    };

    auto addDebugInfo = [](const ast::Clause* clause, Own<ram::Statement> stmt) -> Own<ram::Statement> {
        // Add debug info
        std::ostringstream ds;
        ds << toString(*clause) << "\nin file ";
        ds << clause->getSrcLoc();

        return mk<ram::DebugInfo>(std::move(stmt), ds.str());
    };

    VecOwn<ram::Statement> result;

    // Get relation names
    std::string mainRelation = getConcreteRelationName(rel.getQualifiedName());

    // Iterate over all non-recursive clauses that belong to the relation
    for (auto&& clause : context->getProgram()->getClauses(rel)) {
        // Skip recursive and subsumptive clauses
        if (context->isRecursiveClause(clause) || isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        // Translate each diff version, where the diff_minus/diff_plus occurs in
        // different body atom positions in each diff version
        VecOwn<ram::Statement> clauseDiffVersions = translateNonRecursiveClauseDiffVersions(*clause);
        for (auto& clauseDiffVersion : clauseDiffVersions) {
            appendStmt(result, addDebugInfo(clause, addProfiling(rel, clause, std::move(clauseDiffVersion))));
        }
    }

    // Add logging for entire relation
    if (Global::config().has("profile")) {
        const std::string& relationName = toString(rel.getQualifiedName());
        const auto& srcLocation = rel.getSrcLoc();
        const std::string logSizeStatement = LogStatement::nNonrecursiveRelation(relationName, srcLocation);

        // Add timer if we did any work
        if (!result.empty()) {
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRelation(relationName, srcLocation);
            auto newStmt = mk<ram::LogRelationTimer>(
                    mk<ram::Sequence>(std::move(result)), logTimerStatement, mainRelation);
            result.clear();
            appendStmt(result, std::move(newStmt));
        } else {
            // Add table size printer
            appendStmt(result, mk<ram::LogSize>(mainRelation, logSizeStatement));
        }
    }

    return mk<ram::Sequence>(std::move(result));
}

VecOwn<ram::Statement> UnitTranslator::translateNonRecursiveClauseDiffVersions(const ast::Clause& clause) const {
    // Generate diff versions
    VecOwn<ram::Statement> clauseDiffVersions;

    // for (std::size_t diffVersion = 0; diffVersion < ast::getBodyLiterals<ast::Atom>(clause).size(); diffVersion++) {
    appendStmt(clauseDiffVersions, context->translateNonRecursiveClause(clause, IncrementalDiffMinus));
    appendStmt(clauseDiffVersions, context->translateNonRecursiveClause(clause, IncrementalDiffPlus));
    // }

    return clauseDiffVersions;
}

Own<ram::Relation> UnitTranslator::createRamRelation(
        const ast::Relation* baseRelation, std::string ramRelationName) const {
    auto arity = baseRelation->getArity();
    // auto representation = baseRelation->getRepresentation();
    auto representation = RelationRepresentation::INCREMENTAL;

    // Add in base relation information
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    for (const auto& attribute : baseRelation->getAttributes()) {
        attributeNames.push_back(attribute->getName());
        attributeTypeQualifiers.push_back(context->getAttributeTypeQualifier(attribute->getTypeName()));
    }

    // Add in provenance information
    attributeNames.push_back("@iteration");
    attributeTypeQualifiers.push_back("i:number");

    attributeNames.push_back("@count");
    attributeTypeQualifiers.push_back("i:number");

    return mk<ram::Relation>(
            ramRelationName, arity + 2, 2, attributeNames, attributeTypeQualifiers, representation);
}

VecOwn<ram::Relation> UnitTranslator::createRamRelations(const std::vector<std::size_t>& sccOrdering) const {
    // Regular relations
    auto ramRelations = seminaive::UnitTranslator::createRamRelations(sccOrdering);

    // We need to also generate all the auxiliary relations:
    // - prev
    // - delta_prev
    // - new_prev
    //      - if we keep the normal relation as the updated one, the instead of diff_applied,
    //        we can have prev
    // - temp_diff_plus (updated_diff_plus)
    // - temp_diff_minus (updated_diff_minus)
    //      - check if we can simulate semantics of temp without having extra relations
    //        this might be slow, but do it as an initial step
    // - diff_plus
    // - diff_minus
    // - delta_diff_plus
    // - delta_diff_minus
    // - new_diff_plus
    // - new_diff_minus

    return ramRelations;
}

void UnitTranslator::addAuxiliaryArity(
        const ast::Relation* /* relation */, std::map<std::string, std::string>& directives) const {
    // directives.insert(std::make_pair("auxArity", "2"));
    directives.insert(std::make_pair("auxValues", "0,1"));
}

Own<ram::Statement> UnitTranslator::generateClearExpiredRelations(
        const std::set<const ast::Relation*>& /* expiredRelations */) const {
    // Relations should be preserved if incremental is enabled
    return mk<ram::Sequence>();
}

Own<ram::Statement> UnitTranslator::generateStratumTableUpdates(
        const std::set<const ast::Relation*>& scc) const {
    VecOwn<ram::Statement> updateTable;

    for (const ast::Relation* rel : scc) {
        // Copy @new into main relation, @delta := @new, and empty out @new
        std::string mainRelation = getConcreteRelationName(rel->getQualifiedName());
        std::string newRelation = getNewRelationName(rel->getQualifiedName());
        std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());

        // swap new and and delta relation and clear new relation afterwards (if not a subsumptive relation)
        Own<ram::Statement> updateRelTable;
        if (!context->hasSubsumptiveClause(rel->getQualifiedName())) {
            updateRelTable = mk<ram::Sequence>(mk<ram::Clear>(deltaRelation),
                    generateMergeRelationsWithFilter(rel, deltaRelation, newRelation, mainRelation),
                    generateMergeRelations(rel, mainRelation, newRelation),
                    mk<ram::Clear>(newRelation));

        /* TODO: Handle subsumptive clauses correctly
        } else {
            updateRelTable = generateMergeRelations(rel, mainRelation, deltaRelation);
            */
        }

        // Measure update time
        if (Global::config().has("profile")) {
            updateRelTable = mk<ram::LogRelationTimer>(std::move(updateRelTable),
                    LogStatement::cRecursiveRelation(toString(rel->getQualifiedName()), rel->getSrcLoc()),
                    newRelation);
        }

        appendStmt(updateTable, std::move(updateRelTable));
    }
    return mk<ram::Sequence>(std::move(updateTable));
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
    return stmt;
}

Own<ram::Statement> UnitTranslator::generateMergeRelationsWithFilter(const ast::Relation* rel,
        const std::string& destRelation, const std::string& srcRelation,
        const std::string& filterRelation) const {
    VecOwn<ram::Expression> values;
    VecOwn<ram::Expression> values2;

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity() + 2; i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
        if (i < rel->getArity()) {
            values2.push_back(mk<ram::TupleElement>(0, i));
        } else {
            values2.push_back(mk<ram::UndefValue>());
        }
    }
    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto filtered =
            mk<ram::Filter>(mk<ram::Negation>(mk<ram::ExistenceCheck>(filterRelation, std::move(values2))),
                    std::move(insertion));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(filtered)));
    return stmt;
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

Own<ram::Sequence> UnitTranslator::makeIfStatement(
        Own<ram::Condition> condition, Own<ram::Operation> trueOp, Own<ram::Operation> falseOp) const {
    auto negatedCondition = mk<ram::Negation>(clone(condition));

    auto trueBranch = mk<ram::Query>(mk<ram::Filter>(std::move(condition), std::move(trueOp)));
    auto falseBranch = mk<ram::Query>(mk<ram::Filter>(std::move(negatedCondition), std::move(falseOp)));

    return mk<ram::Sequence>(std::move(trueBranch), std::move(falseBranch));
}

std::map<std::string, Own<ram::Statement>>& UnitTranslator::getRamSubroutines() {
    /*
    std::map<std::string, ram::Statement*> res;

    for (const auto& sub : updateRamSubroutines) {
        res.insert(std::make_pair(sub.first, sub.second.get()));
    }
    */

    return updateRamSubroutines;
}

}  // namespace souffle::ast2ram::incremental::update
