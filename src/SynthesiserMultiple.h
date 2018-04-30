/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SynthesiserMultiple.h
 *
 * Declares synthesiser classes to synthesise C++ code from a RAM program.
 *
 ***********************************************************************/

#pragma once

#include "RamProgram.h"
#include "RamRelation.h"
#include "RamTranslationUnit.h"
#include "SymbolTable.h"

#include "RamStatement.h"

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace souffle {

/**
 * A RAM synthesiser: synthesises a C++ program from a RAM program.
 */
class SynthesiserMultiple {
public:
    SynthesiserMultiple() = default;
    virtual ~SynthesiserMultiple() = default;

    /**
     * Generates the code for a given ram translation unit.
     *
     * @param tu
     * @param os the stream to send the generated C++ code to.
     * @param id the base identifier used in code generation, including class name.
     */
    std::vector<std::string>* generateCode(const RamTranslationUnit& tu, const std::string& id, const std::string& baseFileName) const;
};
}  // end of namespace souffle
