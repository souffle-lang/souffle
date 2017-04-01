/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstTranslationUnit.h
 *
 * Define a class that represents a Datalog translation unit, consisting
 * of a datalog program, error reports and cached analysis results.
 *
 ***********************************************************************/
#pragma once

#include "AstAnalysis.h"
#include "AstProgram.h"
#include "DebugReport.h"
#include "ErrorReport.h"
#include "SymbolTable.h"

#include <map>
#include <memory>

namespace souffle {

class AstTranslationUnit {
private:
    mutable std::map<std::string, std::unique_ptr<AstAnalysis>> analyses;

    /* Program AST */
    std::unique_ptr<AstProgram> program;

    /* The table of symbols encountered in the input program */
    souffle::SymbolTable symbolTable;

    ErrorReport errorReport;

    DebugReport debugReport;

public:
    AstTranslationUnit(std::unique_ptr<AstProgram> program, bool nowarn = false)
            : program(std::move(program)), errorReport(nowarn) {}

    virtual ~AstTranslationUnit() = default;

    template <class Analysis>
    Analysis* getAnalysis() const {
        std::string name = Analysis::name;
        auto it = analyses.find(name);
        if (it == analyses.end()) {
            // analysis does not exist yet, create instance and run it.
            analyses[name] = std::unique_ptr<AstAnalysis>(new Analysis());
            analyses[name]->run(*this);
        }
        return dynamic_cast<Analysis*>(analyses[name].get());
    }

    AstProgram* getProgram() {
        return program.get();
    }

    const AstProgram* getProgram() const {
        return program.get();
    }

    souffle::SymbolTable& getSymbolTable() {
        return symbolTable;
    }

    const souffle::SymbolTable& getSymbolTable() const {
        return symbolTable;
    }

    ErrorReport& getErrorReport() {
        return errorReport;
    }

    const ErrorReport& getErrorReport() const {
        return errorReport;
    }

    void invalidateAnalyses() {
        analyses.clear();
    }

    DebugReport& getDebugReport() {
        return debugReport;
    }

    const DebugReport& getDebugReport() const {
        return debugReport;
    }
};

}  // end of namespace souffle
