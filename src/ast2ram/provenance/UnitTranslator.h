/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ProvenanceTranslator.h
 *
 ***********************************************************************/

#pragma once

#include "ast2ram/seminaive/UnitTranslator.h"

namespace souffle::ast {
class Atom;
class Program;
class Variable;
}  // namespace souffle::ast

namespace souffle::ram {
class Condition;
class ExistenceCheck;
class Node;
class Operation;
class Statement;
class SubroutineReturn;
}  // namespace souffle::ram

namespace souffle::ast2ram {
class ValueIndex;
}

namespace souffle::ast2ram::provenance {

class UnitTranslator : public ast2ram::seminaive::UnitTranslator {
public:
    UnitTranslator() : ast2ram::seminaive::UnitTranslator() {}

protected:
    Own<ram::Sequence> generateProgram(const ast::TranslationUnit& translationUnit) override;
    Own<ram::Statement> generateClearExpiredRelations(
            const std::set<const ast::Relation*>& expiredRelations) const override;
    Own<ram::Relation> createRamRelation(
            const ast::Relation* baseRelation, std::string ramRelationName) const override;
    VecOwn<ram::Relation> createRamRelations(const std::vector<std::size_t>& sccOrdering) const override;
    void addAuxiliaryArity(
            const ast::Relation* relation, std::map<std::string, std::string>& directives) const override;

    Own<ram::Statement> generateMergeRelations(const ast::Relation* rel, const std::string& destRelation,
            const std::string& srcRelation) const override;

private:
    /** Translate RAM code for subroutine to get subproofs */
    Own<ram::Statement> makeSubproofSubroutine(const ast::Clause& clause);

    /** Translate RAM code for subroutine to get subproofs for non-existence of a tuple */
    Own<ram::Statement> makeNegationSubproofSubroutine(const ast::Clause& clause);

    void addProvenanceClauseSubroutines(const ast::Program* program);
    Own<ram::Sequence> generateInfoClauses(const ast::Program* program);

    std::string getInfoRelationName(const ast::Clause* clause) const;

    Own<ram::ExistenceCheck> makeRamAtomExistenceCheck(const ast::Atom* atom,
            const std::map<std::size_t, std::string>& idToVarName, ValueIndex& valueIndex) const;
    Own<ram::SubroutineReturn> makeRamReturnTrue() const;
    Own<ram::SubroutineReturn> makeRamReturnFalse() const;
    void transformVariablesToSubroutineArgs(
            ram::Node* node, const std::map<std::size_t, std::string>& idToVar) const;
    Own<ram::Sequence> makeIfStatement(
            Own<ram::Condition> condition, Own<ram::Operation> trueOp, Own<ram::Operation> falseOp) const;
};

}  // namespace souffle::ast2ram::provenance
