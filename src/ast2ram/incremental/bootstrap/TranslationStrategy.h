/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslationStrategy.h
 *
 * Implementation of the incremental evaluation strategy.
 *
 ***********************************************************************/

#pragma once

#include "ast2ram/TranslationStrategy.h"
#include "souffle/utility/ContainerUtil.h"

namespace souffle::ast2ram {
class ClauseTranslator;
class ConstraintTranslator;
class UnitTranslator;
class TranslatorContext;
class ValueIndex;
class ValueTranslator;
}  // namespace souffle::ast2ram

namespace souffle::ast2ram::incremental::bootstrap {

class TranslationStrategy : public ast2ram::TranslationStrategy {
public:
    std::string getName() const override {
        return "IncrementalEvaluation";
    }

    ast2ram::UnitTranslator* createUnitTranslator() const override;
    ast2ram::ClauseTranslator* createClauseTranslator(
            const TranslatorContext& context, TranslationMode mode) const override;
    ast2ram::ConstraintTranslator* createConstraintTranslator(
            const TranslatorContext& context, const ValueIndex& index) const override;
    ast2ram::ValueTranslator* createValueTranslator(
            const TranslatorContext& context, const ValueIndex& index) const override;
};

}  // namespace souffle::ast2ram::incremental::bootstrap
