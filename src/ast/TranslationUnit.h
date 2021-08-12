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

#include "souffle/utility/DynamicCasting.h"
#include "souffle/utility/Types.h"
#include <map>
#include <utility>

namespace souffle {
class ErrorReport;
class DebugReport;
}  // namespace souffle

namespace souffle::ast {

class Program;

namespace analysis {
class Analysis;
}

/**
 * @class TranslationUnit
 * @brief Translation unit class for the translation pipeline
 *
 * The translation unit class consisting of
 * an symbol table, AST program, error reports, and
 * cached analysis results.
 */

class TranslationUnit {
public:
    TranslationUnit(Own<Program> program, ErrorReport& e, DebugReport& d);
    virtual ~TranslationUnit();

    /** get analysis: analysis is generated on the fly if not present */
    template <class Analysis>
    Analysis* getAnalysis() const {
        auto it = analyses.find(Analysis::name);
        analysis::Analysis* ana = nullptr;
        if (it == analyses.end()) {
            ana = addAnalysis(Analysis::name, mk<Analysis>());
        } else {
            ana = it->second.get();
        }

        return as<Analysis>(ana);
    }

    /** Return the program */
    Program& getProgram() const {
        return *program.get();
    }

    /** Return error report */
    ErrorReport& getErrorReport() {
        return errorReport;
    }

    /** Destroy all cached analyses of translation unit */
    void invalidateAnalyses() const;

    /** Return debug report */
    DebugReport& getDebugReport() {
        return debugReport;
    }

private:
    analysis::Analysis* addAnalysis(char const* name, Own<analysis::Analysis> analysis) const;

private:
    /** Cached analyses */
    mutable std::map<std::string, Own<analysis::Analysis>> analyses;

    /** AST program */
    Own<Program> program;

    /** Error report capturing errors while compiling */
    ErrorReport& errorReport;

    /** HTML debug report */
    DebugReport& debugReport;
};

}  // namespace souffle::ast
