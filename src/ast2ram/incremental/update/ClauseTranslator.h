/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseTranslator.h
 *
 ***********************************************************************/

#pragma once

#include "ast2ram/seminaive/ClauseTranslator.h"
#include <vector>

namespace souffle::ast {
class Atom;
class Clause;
}  // namespace souffle::ast

namespace souffle::ram {
class Operation;
}

namespace souffle::ast2ram {
class TranslatorContext;
}

namespace souffle::ast2ram::incremental::update {

class ClauseTranslator : public ast2ram::seminaive::ClauseTranslator {
public:
    ClauseTranslator(const TranslatorContext& context, TranslationMode mode = DEFAULT)
            : ast2ram::seminaive::ClauseTranslator(context, mode) {}

    /** Allow using the base class translators */
    using ast2ram::seminaive::ClauseTranslator::translateNonRecursiveClause;
    using ast2ram::seminaive::ClauseTranslator::translateRecursiveClause;

    /** Entry points */
    Own<ram::Statement> translateNonRecursiveClause(const ast::Clause& clause) override;
    Own<ram::Statement> translateRecursiveClause(const ast::Clause& clause,
            const std::set<const ast::Relation*>& scc, std::size_t version) override;

protected:
    std::string getClauseAtomName(const ast::Clause& clause, const ast::Atom* atom) const override;

    Own<ram::Operation> addNegatedDeltaAtom(Own<ram::Operation> op, const ast::Atom* atom) const override;
    Own<ram::Operation> addNegatedAtom(
            Own<ram::Operation> op, const ast::Clause& clause, const ast::Atom* atom) const override;
    Own<ram::Operation> addBodyLiteralConstraints(
            const ast::Clause& clause, Own<ram::Operation> op) const override;
    Own<ram::Operation> createInsertion(const ast::Clause& clause) const override;
    void indexAtoms(const ast::Clause& clause) override;
    Own<ram::Operation> addAtomScan(Own<ram::Operation> op, const ast::Atom* atom, const ast::Clause& clause,
            std::size_t curLevel) const override;

    /** This keeps track of which position the diff_plus/diff_minus atom should
     * be in the rule, in a similar manner to how version in
     * seminaive/ClauseTranslator keeps track of the position of the delta
     * atom */
    std::size_t diffVersion{0};

private:
    Own<ram::Expression> getLevelNumber(const ast::Clause& clause) const;

    /** Utility functions for body constraints for incremental update */
    Own<ram::Operation> addEnsureNotExistsInDiffRelationConstraint(Own<ram::Operation> op,
            const ast::Atom* atom, std::string checkRelation, int checkCount, std::size_t curLevel) const;
    Own<ram::Operation> addEnsureExistsInRelationConstraint(Own<ram::Operation> op, const ast::Atom* atom,
            std::string checkRelation, std::size_t curLevel) const;
    Own<ram::Operation> addEnsureEarliestIterationConstraint(Own<ram::Operation> op, const ast::Atom* atom,
            std::size_t curLevel, std::string checkRelation, std::size_t atomIdx) const;
    Own<ram::Operation> addEnsureExistsForDeletionConstraint(
            Own<ram::Operation> op, const ast::Atom* atom, std::size_t curLevel) const;

    Own<ram::Expression> mkIterMinusOne() const;
    bool isRecursiveAtom(const ast::Atom* atom) const;
};

}  // namespace souffle::ast2ram::incremental::update
