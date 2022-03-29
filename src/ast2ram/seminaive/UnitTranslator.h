/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnitTranslator.h
 *
 ***********************************************************************/

#pragma once

#include "ast2ram/UnitTranslator.h"
#include "souffle/utility/ContainerUtil.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace souffle::ast {
class Clause;
class Relation;
class TranslationUnit;
class Atom;
}  // namespace souffle::ast

namespace souffle::ram {
class Relation;
class Sequence;
class Statement;
class TranslationUnit;
}  // namespace souffle::ram

namespace souffle::ast2ram::seminaive {

class UnitTranslator : public ast2ram::UnitTranslator {
public:
    UnitTranslator();
    ~UnitTranslator();

    Own<ram::TranslationUnit> translateUnit(ast::TranslationUnit& tu) override;

protected:
    virtual void addRamSubroutine(std::string subroutineID, Own<ram::Statement> subroutine);
    virtual Own<ram::Relation> createRamRelation(
            const ast::Relation* baseRelation, std::string ramRelationName) const;
    virtual VecOwn<ram::Relation> createRamRelations(const std::vector<std::size_t>& sccOrdering) const;
    Own<ram::Statement> translateRecursiveClauses(
            const std::set<const ast::Relation*>& scc, const ast::Relation* rel) const;
    Own<ram::Statement> translateSubsumptiveRecursiveClauses(
            const std::set<const ast::Relation*>& scc, const ast::Relation* rel) const;
    VecOwn<ram::Statement> generateClauseVersions(
            const ast::Clause* clause, const std::set<const ast::Relation*>& scc) const;
    std::vector<ast::Atom*> getSccAtoms(
            const ast::Clause* clause, const std::set<const ast::Relation*>& scc) const;

    virtual void addAuxiliaryArity(
            const ast::Relation* relation, std::map<std::string, std::string>& directives) const;

    /** -- Generation methods -- */

    /** High-level relation translation */
    virtual Own<ram::Sequence> generateProgram(const ast::TranslationUnit& translationUnit);
    virtual Own<ram::Statement> generateNonRecursiveRelation(const ast::Relation& rel) const;
    Own<ram::Statement> generateRecursiveStratum(const std::set<const ast::Relation*>& scc) const;

    /** IO translation */
    Own<ram::Statement> generateStoreRelation(const ast::Relation* relation) const;
    Own<ram::Statement> generateLoadRelation(const ast::Relation* relation) const;

    /** Low-level stratum translation */
    Own<ram::Statement> generateStratum(std::size_t scc) const;
    virtual Own<ram::Statement> generateStratumPreamble(const std::set<const ast::Relation*>& scc) const;
    Own<ram::Statement> generateNonRecursiveDelete(const std::set<const ast::Relation*>& scc) const;
    Own<ram::Statement> generateStratumPostamble(const std::set<const ast::Relation*>& scc) const;
    Own<ram::Statement> generateStratumLoopBody(const std::set<const ast::Relation*>& scc) const;
    virtual Own<ram::Statement> generateStratumTableUpdates(const std::set<const ast::Relation*>& scc) const;
    Own<ram::Statement> generateStratumExitSequence(const std::set<const ast::Relation*>& scc) const;

    /** Other helper generations */
    virtual Own<ram::Statement> generateClearExpiredRelations(
            const std::set<const ast::Relation*>& expiredRelations) const;
    Own<ram::Statement> generateClearRelation(const ast::Relation* relation) const;
    virtual Own<ram::Statement> generateMergeRelations(
            const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const;
    virtual Own<ram::Statement> generateMergeRelationsWithFilter(const ast::Relation* rel,
            const std::string& destRelation, const std::string& srcRelation,
            const std::string& filterRelation) const;
    virtual Own<ram::Statement> generateEraseTuples(
            const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const;

private:
    std::map<std::string, Own<ram::Statement>> ramSubroutines;
};

}  // namespace souffle::ast2ram::seminaive
