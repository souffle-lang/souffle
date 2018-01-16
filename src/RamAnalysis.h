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

namespace souffle {

class RamTranslationUnit;

/**
 * Abstract class for a RAM Analysis.
 */
class RamAnalysis {
public:
    virtual ~RamAnalysis() = default;

    /** run analysis for a RAM translation unit */
    virtual void run(const RamTranslationUnit& translationUnit) = 0;

    /** print the analysis result in HTML format */
    virtual void print(std::ostream& os) const {}

    /** define output stream operator */
    friend std::ostream& operator<<(std::ostream& out, const RamAnalysis& other) {
        other.print(out);
        return out;
    }
};

}  // end of namespace souffle
