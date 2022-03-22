/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslatorContext.cpp
 *
 ***********************************************************************/

#include "ast2ram/incremental/update/TranslatorContext.h"
#include "ast2ram/incremental/update/ClauseTranslator.h"
#include "ram/Statement.h"

namespace souffle::ast2ram::incremental::update {

Own<ram::Statement> TranslatorContext::translateNonRecursiveClause(
        const ast::Clause& clause, std::size_t diffVersion, TranslationMode mode) const {
    auto clauseTranslator = Own<ast2ram::ClauseTranslator>(translationStrategy->createClauseTranslator(*this, mode));

    if (auto* updateClauseTranslator = as<incremental::update::ClauseTranslator>(clauseTranslator.get())) {
        return updateClauseTranslator->translateNonRecursiveClause(clause, diffVersion);
    }

    std::cerr << "This should never happen, since the ClauseTranslator should always be an incremental::update::ClauseTranslator!!" << std::endl;
    return nullptr;
}

Own<ram::Statement> TranslatorContext::translateRecursiveClause(const ast::Clause& clause,
        const std::set<const ast::Relation*>& scc, std::size_t version,
        std::size_t diffVersion, TranslationMode mode) const {
    auto clauseTranslator = Own<ast2ram::ClauseTranslator>(translationStrategy->createClauseTranslator(*this, mode));

    if (auto* updateClauseTranslator = as<incremental::update::ClauseTranslator>(clauseTranslator.get())) {
        return updateClauseTranslator->translateRecursiveClause(clause, scc, version, diffVersion);
    }
    std::cerr << "This should never happen, since the ClauseTranslator should always be an incremental::update::ClauseTranslator!!" << std::endl;
    return nullptr;
}

    /*
TranslatorContext::TranslatorContext(const ast::TranslationUnit& tu) {
    souffle::ast2ram::TranslatorContext(tu);

    std::cout << "Making ast2ram::incremental::update::TranslatorContext!!!!" << std::endl;

    translationStrategy = mk<incremental::update::TranslationStrategy>();
}
*/

} // namespace souffle::ast2ram::incremental::update
