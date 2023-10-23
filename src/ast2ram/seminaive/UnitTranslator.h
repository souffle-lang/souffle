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
#include "ram/Expression.h"
#include "souffle/utility/ContainerUtil.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace souffle {
class Global;
}

namespace souffle::ast {
class Clause;
class Relation;
struct NameComparison;
using RelationSet = std::set<const Relation*, NameComparison>;
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
    void addRamSubroutine(std::string subroutineID, Own<ram::Statement> subroutine);
    virtual Own<ram::Relation> createRamRelation(
            const ast::Relation* baseRelation, std::string ramRelationName) const;
    virtual VecOwn<ram::Relation> createRamRelations(const std::vector<std::size_t>& sccOrdering) const;
    Own<ram::Statement> translateRecursiveClauses(
            const ast::RelationSet& scc, const ast::Relation* rel) const;
    Own<ram::Statement> translateSubsumptiveRecursiveClauses(
            const ast::RelationSet& scc, const ast::Relation* rel) const;
    VecOwn<ram::Statement> generateClauseVersions(
            const ast::Clause* clause, const ast::RelationSet& scc) const;
    std::vector<ast::Atom*> getSccAtoms(const ast::Clause* clause, const ast::RelationSet& scc) const;

    virtual void addAuxiliaryArity(
            const ast::Relation* relation, std::map<std::string, std::string>& directives) const;

    /** -- Generation methods -- */

    /** High-level relation translation */
    virtual Own<ram::Sequence> generateProgram(const ast::TranslationUnit& translationUnit);
    Own<ram::Statement> generateNonRecursiveRelation(const ast::Relation& rel) const;
    Own<ram::Statement> generateRecursiveStratum(const ast::RelationSet& scc, std::size_t sccNum) const;

    /** IO translation */
    Own<ram::Statement> generateStoreRelation(const ast::Relation* relation) const;
    Own<ram::Statement> generateLoadRelation(const ast::Relation* relation) const;

    /** Low-level stratum translation */
    Own<ram::Statement> generateStratum(std::size_t scc) const;
    Own<ram::Statement> generateStratumPreamble(const ast::RelationSet& scc) const;
    Own<ram::Statement> generateNonRecursiveDelete(const ast::RelationSet& scc) const;
    Own<ram::Statement> generateStratumPostamble(const ast::RelationSet& scc) const;
    Own<ram::Statement> generateStratumLoopBody(const ast::RelationSet& scc) const;
    Own<ram::Statement> generateStratumTableUpdates(const ast::RelationSet& scc) const;
    Own<ram::Statement> generateStratumExitSequence(const ast::RelationSet& scc) const;

    /** Other helper generations */
    virtual Own<ram::Statement> generateClearExpiredRelations(const ast::RelationSet& expiredRelations) const;
    Own<ram::Statement> generateClearRelation(const ast::Relation* relation) const;
    virtual Own<ram::Statement> generateMergeRelations(
            const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const;
    virtual Own<ram::Statement> generateMergeRelationsWithFilter(const ast::Relation* rel,
            const std::string& destRelation, const std::string& srcRelation,
            const std::string& filterRelation) const;
    virtual Own<ram::Statement> generateEraseTuples(
            const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const;
    virtual Own<ram::Statement> generateDebugRelation(const ast::Relation* rel,
            const std::string& destRelation, const std::string& srcRelation,
            Own<ram::Expression> iteration) const;

private:
    std::map<std::string, Own<ram::Statement>> ramSubroutines;

    Global* glb;
};

}  // namespace souffle::ast2ram::seminaive
