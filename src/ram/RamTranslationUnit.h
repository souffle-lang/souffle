/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTranslationUnit.h
 *
 * Define a class that represents a Datalog translation unit, consisting
 * of a datalog program, error reports and cached analysis results.
 *
 ***********************************************************************/

#pragma once

#include "DebugReport.h"
#include "Global.h"
#include "SymbolTable.h"
#include "ram/RamProgram.h"
#include "ram/analysis/RamAnalysis.h"
#include <cassert>
#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace souffle {
class ErrorReport;

/**
 * @class RamTranslationUnit
 * @brief Translating a RAM program
 *
 * Comprises the program, symbol table, error report and debug report
 */
class RamTranslationUnit {
public:
    RamTranslationUnit(std::unique_ptr<RamProgram> prog, SymbolTable sym, ErrorReport& e, DebugReport& d)
            : program(std::move(prog)), symbolTable(std::move(sym)), errorReport(e), debugReport(d) {
        assert(program != nullptr && "program is a null-pointer");
    }

    virtual ~RamTranslationUnit() = default;

    /** @brief templated method to compute/retrieve an analysis for a translation unit */
    template <class Analysis>
    Analysis* getAnalysis() const {
        static const bool debug = Global::config().has("debug-report");
        std::string name = Analysis::name;
        auto it = analyses.find(name);
        if (it == analyses.end()) {
            // analysis does not exist yet, create instance and run it.
            auto analysis = std::make_unique<Analysis>(Analysis::name);
            analysis->run(*this);
            // output analysis in debug report
            if (debug) {
                std::stringstream ramAnalysisStr;
                ramAnalysisStr << *analysis;
                if (!ramAnalysisStr.str().empty()) {
                    debugReport.addSection(
                            analysis->getName(), "RAM Analysis " + analysis->getName(), ramAnalysisStr.str());
                }
            }
            // check it hasn't been created by someone else, and insert if not
            it = analyses.find(name);
            if (it == analyses.end()) {
                analyses[name] = std::move(analysis);
            }
        }
        return dynamic_cast<Analysis*>(analyses[name].get());
    }

    /** @brief get the set of alive analyses of the translation unit */
    std::set<const RamAnalysis*> getAliveAnalyses() const {
        std::set<const RamAnalysis*> result;
        for (auto const& a : analyses) {
            result.insert(a.second.get());
        }
        return result;
    }

    /** @brief throw away all alive analyses of the translation unit */
    void invalidateAnalyses() {
        analyses.clear();
    }

    /** @brief get the RAM Program of the translation unit  */
    const RamProgram& getProgram() const {
        return *program;
    }

    /** @brief get the RAM Program of the translation unit  */
    RamProgram& getProgram() {
        return *program;
    }

    /** @brief get symbol table  */
    souffle::SymbolTable& getSymbolTable() {
        return symbolTable;
    }

    /** @brief get error report */
    ErrorReport& getErrorReport() {
        return errorReport;
    }

    /** @brief get error report */
    const ErrorReport& getErrorReport() const {
        return errorReport;
    }

    /** @brief get debug report */
    DebugReport& getDebugReport() {
        return debugReport;
    }

    /** @brief get const debug report */
    const DebugReport& getDebugReport() const {
        return debugReport;
    }

protected:
    /* cached analyses */
    mutable std::map<std::string, std::unique_ptr<RamAnalysis>> analyses;

    /* Program RAM */
    std::unique_ptr<RamProgram> program;

    /* The table of symbols encountered in the input program */
    souffle::SymbolTable symbolTable;

    /* Error report for raising errors and warnings */
    ErrorReport& errorReport;

    /* Debug report for logging information */
    DebugReport& debugReport;
};

}  // end of namespace souffle
