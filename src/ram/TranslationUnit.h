/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslationUnit.h
 *
 * Define a RAM translation unit
 *
 ***********************************************************************/

#pragma once

#include "TranslationUnitBase.h"

namespace souffle::ram {

class Program;

/**
 * @class TranslationUnit
 * @brief Translating a RAM program
 *
 * Comprises the program, symbol table, error report, debug report, and analyses
 */
class TranslationUnit final : public souffle::detail::TranslationUnitBase<TranslationUnit, Program> {
    using Base = souffle::detail::TranslationUnitBase<TranslationUnit, Program>;

public:
    using Base::Base;

protected:
    void logAnalysis(Analysis&) const override;
};

namespace analysis {
using Analysis = souffle::ram::TranslationUnit::Analysis;
}

}  // namespace souffle::ram
