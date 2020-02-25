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
 * Define AST translation unit class consisting of
 * an symbol table, AST program, error reports, and
 * cached analysis results.
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

/**
 * AstTranslationUnit class
 */

class AstTranslationUnit {
public:
    AstTranslationUnit(std::unique_ptr<AstProgram> program, ErrorReport& e, DebugReport& d)
            : program(std::move(program)), errorReport(e), debugReport(d) {}

    virtual ~AstTranslationUnit() = default;

    /** get analysis: analysis is generated on the fly if not present */
    template <class Analysis>
    Analysis* getAnalysis() const {
        std::string name = Analysis::name;
        auto it = analyses.find(name);
        if (it == analyses.end()) {
            // analysis does not exist yet, create instance and run it.
            analyses[name] = std::make_unique<Analysis>();
            analyses[name]->run(*this);
        }
        return dynamic_cast<Analysis*>(analyses[name].get());
    }

    /** get the AST program */
    AstProgram* getProgram() {
        return program.get();
    }

    /** get the AST program */
    const AstProgram* getProgram() const {
        return program.get();
    }

    /** get error report */
    ErrorReport& getErrorReport() {
        return errorReport;
    }

    /** get error report */
    const ErrorReport& getErrorReport() const {
        return errorReport;
    }

    /** destroy all cached analyses of translation unit */
    void invalidateAnalyses() {
        analyses.clear();
    }

    /** get debug report */
    DebugReport& getDebugReport() {
        return debugReport;
    }

    /** get debug report */
    const DebugReport& getDebugReport() const {
        return debugReport;
    }

private:
    /** cached analyses */
    mutable std::map<std::string, std::unique_ptr<AstAnalysis>> analyses;

    /** AST program */
    std::unique_ptr<AstProgram> program;

    /** Error report capturing errors while compiling */
    ErrorReport& errorReport;

    /** HTML debug report */
    DebugReport& debugReport;
};

}  // end of namespace souffle
