/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstAnalysis.h
 *
 * Defines an interface for AST analysis
 *
 ***********************************************************************/

#pragma once

#include <ostream>
#include <string>
#include <utility>

namespace souffle {

class AstTranslationUnit;

/**
 * Abstract class for a AST Analysis.
 */
class AstAnalysis {
public:
    AstAnalysis(std::string identifier) : identifier(std::move(identifier)) {}
    virtual ~AstAnalysis() = default;

    /** run analysis for a Ast translation unit */
    virtual void run(const AstTranslationUnit& /*translationUnit*/) = 0;

    /** print the analysis result in HTML format */
    virtual void print(std::ostream&) const {}

    /** @brief get name of the analysis */
    virtual const std::string& getName() const {
        return identifier;
    }

    /** define output stream operator */
    friend std::ostream& operator<<(std::ostream& out, const AstAnalysis& other) {
        other.print(out);
        return out;
    }

protected:
    const std::string identifier;
};

}  // end of namespace souffle
