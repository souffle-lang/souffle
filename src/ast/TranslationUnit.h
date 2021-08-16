/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslationUnit.h
 *
 * Defines the translation unit class
 *
 ***********************************************************************/

#pragma once

#include "TranslationUnitBase.h"

namespace souffle::ast {

class Program;

/**
 * @class TranslationUnit
 * @brief Translation unit class for the translation pipeline
 *
 * The translation unit class consisting of
 * an symbol table, AST program, error reports, and
 * cached analysis results.
 */

class TranslationUnit final : public souffle::detail::TranslationUnitBase<TranslationUnit, Program> {
    using Base = souffle::detail::TranslationUnitBase<TranslationUnit, Program>;

public:
    using Base::Base;

protected:
    void logAnalysis(Analysis&) const override;
};

namespace analysis {
using Analysis = souffle::ast::TranslationUnit::Analysis;
}

}  // namespace souffle::ast
