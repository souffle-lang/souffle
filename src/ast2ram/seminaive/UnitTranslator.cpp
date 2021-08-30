/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnitTranslator.cpp
 *
 ***********************************************************************/

#include "ast2ram/seminaive/UnitTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/Clause.h"
#include "ast/Directive.h"
#include "ast/Relation.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/TopologicallySortedSCCGraph.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/EmptinessCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/Extend.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/Insert.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/Negation.h"
#include "ram/Parallel.h"
#include "ram/Program.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/RelationSize.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/Swap.h"
#include "ram/TranslationUnit.h"
#include "ram/TupleElement.h"
#include "ram/utility/Utils.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast2ram::seminaive {

UnitTranslator::UnitTranslator() : ast2ram::UnitTranslator() {}

UnitTranslator::~UnitTranslator() = default;

void UnitTranslator::addRamSubroutine(std::string subroutineID, Own<ram::Statement> subroutine) {
    assert(!contains(ramSubroutines, subroutineID) && "subroutine ID should not already exist");
    ramSubroutines[subroutineID] = std::move(subroutine);
}

Own<ram::Statement> UnitTranslator::generateClearRelation(const ast::Relation* relation) const {
    return mk<ram::Clear>(getConcreteRelationName(relation->getQualifiedName()));
}

Own<ram::Statement> UnitTranslator::generateNonRecursiveRelation(const ast::Relation& rel) const {
    VecOwn<ram::Statement> result;
    std::string relName = getConcreteRelationName(rel.getQualifiedName());

    // Iterate over all non-recursive clauses that belong to the relation
    for (const auto* clause : context->getClauses(rel.getQualifiedName())) {
        // Skip recursive rules
        if (context->isRecursiveClause(clause)) {
            continue;
        }

        // Translate clause
        Own<ram::Statement> rule = context->translateNonRecursiveClause(*clause);

        // Add logging
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const auto& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = mk<ram::LogRelationTimer>(std::move(rule), logTimerStatement, relName);
        }

        // Add debug info
        std::ostringstream ds;
        ds << toString(*clause) << "\nin file ";
        ds << clause->getSrcLoc();
        rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

        // Add rule to result
        appendStmt(result, std::move(rule));
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
                    mk<ram::Sequence>(std::move(result)), logTimerStatement, relName);
            result.clear();
            appendStmt(result, std::move(newStmt));
        } else {
            // Add table size printer
            appendStmt(result, mk<ram::LogSize>(relName, logSizeStatement));
        }
    }

    return mk<ram::Sequence>(std::move(result));
}

Own<ram::Statement> UnitTranslator::generateStratum(std::size_t scc) const {
    // Make a new ram statement for the current SCC
    VecOwn<ram::Statement> current;

    // load all internal input relations from the facts dir with a .facts extension
    for (const auto& relation : context->getInputRelationsInSCC(scc)) {
        appendStmt(current, generateLoadRelation(relation));
    }

    // Compute the current stratum
    const auto& sccRelations = context->getRelationsInSCC(scc);
    if (context->isRecursiveSCC(scc)) {
        appendStmt(current, generateRecursiveStratum(sccRelations));
    } else {
        assert(sccRelations.size() == 1 && "only one relation should exist in non-recursive stratum");
        const auto* relation = *sccRelations.begin();
        appendStmt(current, generateNonRecursiveRelation(*relation));
    }

    // Store all internal output relations to the output dir with a .csv extension
    for (const auto& relation : context->getOutputRelationsInSCC(scc)) {
        appendStmt(current, generateStoreRelation(relation));
    }

    return mk<ram::Sequence>(std::move(current));
}

Own<ram::Statement> UnitTranslator::generateClearExpiredRelations(
        const std::set<const ast::Relation*>& expiredRelations) const {
    VecOwn<ram::Statement> stmts;
    for (const auto& relation : expiredRelations) {
        appendStmt(stmts, generateClearRelation(relation));
    }
    return mk<ram::Sequence>(std::move(stmts));
}

Own<ram::Statement> UnitTranslator::generateMergeRelations(
        const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const {
    VecOwn<ram::Expression> values;

    // Proposition - insert if not empty
    if (rel->getArity() == 0) {
        auto insertion = mk<ram::Insert>(destRelation, std::move(values));
        return mk<ram::Query>(mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::EmptinessCheck>(srcRelation)), std::move(insertion)));
    }

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity(); i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
    }
    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(insertion)));
    return stmt;
}

Own<ram::Statement> UnitTranslator::translateRecursiveClauses(
        const std::set<const ast::Relation*>& scc, const ast::Relation* rel) const {
    assert(contains(scc, rel) && "relation should belong to scc");
    VecOwn<ram::Statement> result;

    // Translate each recursive clasue
    for (const auto* clause : context->getClauses(rel->getQualifiedName())) {
        // Skip non-recursive clauses
        if (!context->isRecursiveClause(clause)) {
            continue;
        }

        auto clauseVersions = generateClauseVersions(clause, scc);
        for (auto& clauseVersion : clauseVersions) {
            appendStmt(result, std::move(clauseVersion));
        }
    }

    return mk<ram::Sequence>(std::move(result));
}

VecOwn<ram::Statement> UnitTranslator::generateClauseVersions(
        const ast::Clause* clause, const std::set<const ast::Relation*>& scc) const {
    const auto& sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(*clause),
            [&](const ast::Atom* atom) { return contains(scc, context->getAtomRelation(atom)); });

    // Create each version
    VecOwn<ram::Statement> clauseVersions;
    for (std::size_t version = 0; version < sccAtoms.size(); version++) {
        appendStmt(clauseVersions, context->translateRecursiveClause(*clause, scc, version));
    }

    // Check that the correct number of versions have been created
    if (clause->getExecutionPlan() != nullptr) {
        int maxVersion = -1;
        for (const auto& cur : clause->getExecutionPlan()->getOrders()) {
            maxVersion = std::max(cur.first, maxVersion);
        }
        assert((int)sccAtoms.size() > maxVersion && "missing clause versions");
    }

    return clauseVersions;
}

Own<ram::Statement> UnitTranslator::generateStratumPreamble(const std::set<const ast::Relation*>& scc) const {
    VecOwn<ram::Statement> preamble;
    for (const ast::Relation* rel : scc) {
        // Generate code for the non-recursive part of the relation */
        appendStmt(preamble, generateNonRecursiveRelation(*rel));

        // Copy the result into the delta relation
        std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());
        std::string mainRelation = getConcreteRelationName(rel->getQualifiedName());
        appendStmt(preamble, generateMergeRelations(rel, deltaRelation, mainRelation));
    }
    return mk<ram::Sequence>(std::move(preamble));
}

Own<ram::Statement> UnitTranslator::generateStratumPostamble(
        const std::set<const ast::Relation*>& scc) const {
    VecOwn<ram::Statement> postamble;
    for (const ast::Relation* rel : scc) {
        // Drop temporary tables after recursion
        appendStmt(postamble, mk<ram::Clear>(getDeltaRelationName(rel->getQualifiedName())));
        appendStmt(postamble, mk<ram::Clear>(getNewRelationName(rel->getQualifiedName())));
    }
    return mk<ram::Sequence>(std::move(postamble));
}

Own<ram::Statement> UnitTranslator::generateStratumTableUpdates(
        const std::set<const ast::Relation*>& scc) const {
    VecOwn<ram::Statement> updateTable;
    for (const ast::Relation* rel : scc) {
        // Copy @new into main relation, @delta := @new, and empty out @new
        std::string mainRelation = getConcreteRelationName(rel->getQualifiedName());
        std::string newRelation = getNewRelationName(rel->getQualifiedName());
        std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());
        Own<ram::Statement> updateRelTable =
                mk<ram::Sequence>(generateMergeRelations(rel, mainRelation, newRelation),
                        mk<ram::Swap>(deltaRelation, newRelation), mk<ram::Clear>(newRelation));

        if (rel->getRepresentation() == RelationRepresentation::EQREL) {
            updateRelTable = mk<ram::Sequence>(
                    std::move(updateRelTable), mk<ram::Extend>(deltaRelation, mainRelation));
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

Own<ram::Statement> UnitTranslator::generateStratumLoopBody(const std::set<const ast::Relation*>& scc) const {
    VecOwn<ram::Statement> loopBody;
    for (const ast::Relation* rel : scc) {
        auto relClauses = translateRecursiveClauses(scc, rel);

        // add profiling information
        if (Global::config().has("profile")) {
            const std::string& relationName = toString(rel->getQualifiedName());
            const auto& srcLocation = rel->getSrcLoc();
            const std::string logTimerStatement = LogStatement::tRecursiveRelation(relationName, srcLocation);
            relClauses = mk<ram::LogRelationTimer>(mk<ram::Sequence>(std::move(relClauses)),
                    logTimerStatement, getNewRelationName(rel->getQualifiedName()));
        }

        appendStmt(loopBody, mk<ram::Sequence>(std::move(relClauses)));
    }
    return mk<ram::Sequence>(std::move(loopBody));
}

Own<ram::Statement> UnitTranslator::generateStratumExitSequence(
        const std::set<const ast::Relation*>& scc) const {
    // Helper function to add a new term to a conjunctive condition
    auto addCondition = [&](Own<ram::Condition>& cond, Own<ram::Condition> term) {
        cond = (cond == nullptr) ? std::move(term) : mk<ram::Conjunction>(std::move(cond), std::move(term));
    };

    VecOwn<ram::Statement> exitConditions;

    // (1) if all relations in the scc are empty
    Own<ram::Condition> emptinessCheck;
    for (const ast::Relation* rel : scc) {
        addCondition(emptinessCheck, mk<ram::EmptinessCheck>(getNewRelationName(rel->getQualifiedName())));
    }
    appendStmt(exitConditions, mk<ram::Exit>(std::move(emptinessCheck)));

    // (2) if the size limit has been reached for any limitsize relations
    for (const ast::Relation* rel : scc) {
        if (context->hasSizeLimit(rel)) {
            Own<ram::Condition> limit = mk<ram::Constraint>(BinaryConstraintOp::GE,
                    mk<ram::RelationSize>(getConcreteRelationName(rel->getQualifiedName())),
                    mk<ram::SignedConstant>(context->getSizeLimit(rel)));
            appendStmt(exitConditions, mk<ram::Exit>(std::move(limit)));
        }
    }

    return mk<ram::Sequence>(std::move(exitConditions));
}

/** generate RAM code for recursive relations in a strongly-connected component */
Own<ram::Statement> UnitTranslator::generateRecursiveStratum(
        const std::set<const ast::Relation*>& scc) const {
    assert(!scc.empty() && "scc set should not be empty");
    VecOwn<ram::Statement> result;

    // Add in the preamble
    appendStmt(result, generateStratumPreamble(scc));

    // Add in the main fixpoint loop
    auto loopBody = mk<ram::Parallel>(generateStratumLoopBody(scc));
    auto exitSequence = generateStratumExitSequence(scc);
    auto updateSequence = generateStratumTableUpdates(scc);
    auto fixpointLoop = mk<ram::Loop>(
            mk<ram::Sequence>(std::move(loopBody), std::move(exitSequence), std::move(updateSequence)));
    appendStmt(result, std::move(fixpointLoop));

    // Add in the postamble
    appendStmt(result, generateStratumPostamble(scc));

    return mk<ram::Sequence>(std::move(result));
}

void UnitTranslator::addAuxiliaryArity(
        const ast::Relation* /* relation */, std::map<std::string, std::string>& directives) const {
    directives.insert(std::make_pair("auxArity", "0"));
}

Own<ram::Statement> UnitTranslator::generateLoadRelation(const ast::Relation* relation) const {
    VecOwn<ram::Statement> loadStmts;
    for (const auto* load : context->getLoadDirectives(relation->getQualifiedName())) {
        // Set up the corresponding directive map
        std::map<std::string, std::string> directives;
        for (const auto& [key, value] : load->getParameters()) {
            directives.insert(std::make_pair(key, unescape(value)));
        }
        if (Global::config().has("no-warn")) {
            directives.insert(std::make_pair("no-warn", "true"));
        }
        addAuxiliaryArity(relation, directives);

        // Create the resultant load statement, with profile information
        std::string ramRelationName = getConcreteRelationName(relation->getQualifiedName());
        Own<ram::Statement> loadStmt = mk<ram::IO>(ramRelationName, directives);
        if (Global::config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationLoadTime(ramRelationName, relation->getSrcLoc());
            loadStmt = mk<ram::LogRelationTimer>(std::move(loadStmt), logTimerStatement, ramRelationName);
        }
        appendStmt(loadStmts, std::move(loadStmt));
    }
    return mk<ram::Sequence>(std::move(loadStmts));
}

Own<ram::Statement> UnitTranslator::generateStoreRelation(const ast::Relation* relation) const {
    VecOwn<ram::Statement> storeStmts;
    for (const auto* store : context->getStoreDirectives(relation->getQualifiedName())) {
        // Set up the corresponding directive map
        std::map<std::string, std::string> directives;
        for (const auto& [key, value] : store->getParameters()) {
            directives.insert(std::make_pair(key, unescape(value)));
        }
        addAuxiliaryArity(relation, directives);

        // Create the resultant store statement, with profile information
        std::string ramRelationName = getConcreteRelationName(relation->getQualifiedName());
        Own<ram::Statement> storeStmt = mk<ram::IO>(ramRelationName, directives);
        if (Global::config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationSaveTime(ramRelationName, relation->getSrcLoc());
            storeStmt = mk<ram::LogRelationTimer>(std::move(storeStmt), logTimerStatement, ramRelationName);
        }
        appendStmt(storeStmts, std::move(storeStmt));
    }
    return mk<ram::Sequence>(std::move(storeStmts));
}

Own<ram::Relation> UnitTranslator::createRamRelation(
        const ast::Relation* baseRelation, std::string ramRelationName) const {
    auto arity = baseRelation->getArity();
    auto representation = baseRelation->getRepresentation();

    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    for (const auto& attribute : baseRelation->getAttributes()) {
        attributeNames.push_back(attribute->getName());
        attributeTypeQualifiers.push_back(context->getAttributeTypeQualifier(attribute->getTypeName()));
    }

    return mk<ram::Relation>(
            ramRelationName, arity, 0, attributeNames, attributeTypeQualifiers, representation);
}

VecOwn<ram::Relation> UnitTranslator::createRamRelations(const std::vector<std::size_t>& sccOrdering) const {
    VecOwn<ram::Relation> ramRelations;
    for (const auto& scc : sccOrdering) {
        bool isRecursive = context->isRecursiveSCC(scc);
        for (const auto& rel : context->getRelationsInSCC(scc)) {
            // Add main relation
            std::string mainName = getConcreteRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, mainName));

            // Recursive relations also require @delta and @new variants, with the same signature
            if (isRecursive) {
                // Add delta relation
                std::string deltaName = getDeltaRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, deltaName));

                // Add new relation
                std::string newName = getNewRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newName));
            }
        }
    }
    return ramRelations;
}

Own<ram::Sequence> UnitTranslator::generateProgram(const ast::TranslationUnit& translationUnit) {
    // Check if trivial program
    if (context->getNumberOfSCCs() == 0) {
        return mk<ram::Sequence>();
    }
    const auto& sccOrdering =
            translationUnit.getAnalysis<ast::analysis::TopologicallySortedSCCGraphAnalysis>()->order();

    // Create subroutines for each SCC according to topological order
    for (std::size_t i = 0; i < sccOrdering.size(); i++) {
        // Generate the main stratum code
        auto stratum = generateStratum(sccOrdering.at(i));

        // Clear expired relations
        const auto& expiredRelations = context->getExpiredRelations(i);
        stratum = mk<ram::Sequence>(std::move(stratum), generateClearExpiredRelations(expiredRelations));

        // Add the subroutine
        std::string stratumID = "stratum_" + toString(i);
        addRamSubroutine(stratumID, std::move(stratum));
    }

    // Invoke all strata
    VecOwn<ram::Statement> res;
    for (std::size_t i = 0; i < sccOrdering.size(); i++) {
        appendStmt(res, mk<ram::Call>("stratum_" + toString(i)));
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

Own<ram::TranslationUnit> UnitTranslator::translateUnit(ast::TranslationUnit& tu) {
    /* -- Set-up -- */
    auto ram_start = std::chrono::high_resolution_clock::now();
    context = mk<TranslatorContext>(tu);

    /* -- Translation -- */
    // Generate the RAM program code
    auto ramMain = generateProgram(tu);

    // Create the relevant RAM relations
    const auto& sccOrdering = tu.getAnalysis<ast::analysis::TopologicallySortedSCCGraphAnalysis>()->order();
    auto ramRelations = createRamRelations(sccOrdering);

    // Combine all parts into the final RAM program
    ErrorReport& errReport = tu.getErrorReport();
    DebugReport& debugReport = tu.getDebugReport();
    auto ramProgram =
            mk<ram::Program>(std::move(ramRelations), std::move(ramMain), std::move(ramSubroutines));

    // Add the translated program to the debug report
    if (Global::config().has("debug-report")) {
        auto ram_end = std::chrono::high_resolution_clock::now();
        std::string runtimeStr =
                "(" + std::to_string(std::chrono::duration<double>(ram_end - ram_start).count()) + "s)";
        std::stringstream ramProgramStr;
        ramProgramStr << *ramProgram;
        debugReport.addSection("ram-program", "RAM Program " + runtimeStr, ramProgramStr.str());
    }

    // Wrap the program into a translation unit
    return mk<ram::TranslationUnit>(std::move(ramProgram), errReport, debugReport);
}

}  // namespace souffle::ast2ram::seminaive
