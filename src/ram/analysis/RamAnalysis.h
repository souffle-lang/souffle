/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamAnalysis.h
 *
 * Defines an interface for RAM analysis
 *
 ***********************************************************************/

#pragma once

#include <iostream>
#include <string>

namespace souffle {

class RamTranslationUnit;

/**
 * @class RamAnalysis
 * @brief Abstract class for a RAM Analysis.
 */
class RamAnalysis {
public:
    RamAnalysis(const char* id) : identifier(id) {}
    virtual ~RamAnalysis() = default;

    /** @brief get name of the analysis */
    virtual const std::string& getName() const {
        return identifier;
    }

    /** @brief Run analysis for a RAM translation unit */
    virtual void run(const RamTranslationUnit& translationUnit) = 0;

    /** @brief Print the analysis result in HTML format */
    virtual void print(std::ostream& /* os */) const {}

    /** @brief define output stream operator */
    friend std::ostream& operator<<(std::ostream& out, const RamAnalysis& other) {
        other.print(out);
        return out;
    }

protected:
    /** @brief name of analysis instance */
    std::string identifier;
};

}  // end of namespace souffle
