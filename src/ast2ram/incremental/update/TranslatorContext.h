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

#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/incremental/update/TranslationStrategy.h"

namespace souffle::ast2ram::incremental::update {

class TranslatorContext : public ast2ram::TranslatorContext {
public:
    TranslatorContext(const ast::TranslationUnit& tu) 
            : ast2ram::TranslatorContext(tu) {
        std::cout << "Making ast2ram::incremental::update::TranslatorContext!!!!" << std::endl;

        translationStrategy = mk<incremental::update::TranslationStrategy>();
    }
};

} // namespace souffle::ast2ram::incremental::update
