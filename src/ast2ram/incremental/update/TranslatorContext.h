/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslatorContext.h
 *
 * A subclass of ast2ram/utility/TranslatorContext.h, with methods for
 * translating clauses specific for incremental update
 *
 ***********************************************************************/

#include "ast2ram/incremental/update/TranslationStrategy.h"
#include "ast2ram/utility/TranslatorContext.h"

namespace souffle::ast2ram::incremental::update {

class TranslatorContext : public ast2ram::TranslatorContext {
public:
    TranslatorContext(const ast::TranslationUnit& tu) : ast2ram::TranslatorContext(tu) {
        translationStrategy = mk<incremental::update::TranslationStrategy>();
    }

    /** Versions of clause translations which allow for diffVersions */
    Own<ram::Statement> translateNonRecursiveClause(
            const ast::Clause& clause, std::size_t diffVersion, TranslationMode mode = DEFAULT) const;
    Own<ram::Statement> translateRecursiveClause(const ast::Clause& clause,
            const std::set<const ast::Relation*>& scc, std::size_t version, std::size_t diffVersion,
            TranslationMode mode = DEFAULT) const;
};

}  // namespace souffle::ast2ram::incremental::update
