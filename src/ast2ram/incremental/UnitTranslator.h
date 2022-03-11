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
 * Provides the program translator functions for incremental evaluation
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

namespace souffle::ast2ram::incremental {

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
    // TODO: This needs to do incremental update
    void addProvenanceClauseSubroutines(const ast::Program* program);

    Own<ram::ExistenceCheck> makeRamAtomExistenceCheck(const ast::Atom* atom,
            const std::map<int, std::string>& idToVarName, ValueIndex& valueIndex) const;
    Own<ram::SubroutineReturn> makeRamReturnTrue() const;
    Own<ram::SubroutineReturn> makeRamReturnFalse() const;
    void transformVariablesToSubroutineArgs(ram::Node* node, const std::map<int, std::string>& idToVar) const;
    Own<ram::Sequence> makeIfStatement(
            Own<ram::Condition> condition, Own<ram::Operation> trueOp, Own<ram::Operation> falseOp) const;
};

}  // namespace souffle::ast2ram::incremental
