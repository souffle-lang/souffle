/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamExecutor.h
 *
 * Declares entities capable of executing a RAM program.
 *
 ***********************************************************************/

#pragma once

#include "RamData.h"
#include "RamProgram.h"
#include "RamRelation.h"
#include "SymbolTable.h"

#include "RamStatement.h"

#include <ostream>
#include <vector>

namespace souffle {

/**
 * An abstract base class for executing/synthesizing RAM programs
 */

class RamExecutor {
protected:
    /** An optional stream to print logging information to an output stream */
    std::ostream* report;

public:
    RamExecutor() : report(nullptr) {}

    /** A virtual destructor to support safe inheritance */
    virtual ~RamExecutor() = default;

    /**
     * Updates the target this executor is reporting to.
     */
    void setReportTarget(std::ostream& report) {
        this->report = &report;
    }

    /**
     * Disables the reporting. No more report messages will be printed.
     */
    void disableReporting() {
        report = nullptr;
    }

    /**
     * Runs the given RAM statement on an empty environment and returns
     * this environment after the completion of the execution.
     */
    std::unique_ptr<RamEnvironment> execute(SymbolTable& table, const RamProgram& prog) const {
        auto env = std::make_unique<RamEnvironment>(table);
        applyOn(prog, *env, nullptr);
        return env;
    }

    /**
     * Runs the given RAM statement on an empty environment and input data and returns
     * this environment after the completion of the execution.
     */
    std::unique_ptr<RamEnvironment> execute(SymbolTable& table, const RamProgram& prog, RamData* data) const {
        // Ram env managed by the interface
        auto env = std::make_unique<RamEnvironment>(table);
        applyOn(prog, *env, data);
        return env;
    }

    /**
     * Runs a subroutine of a RamProgram
     */
    virtual void executeSubroutine(RamEnvironment& env, const RamStatement& stmt,
            const std::vector<RamDomain>& arguments, std::vector<RamDomain>& returnValues,
            std::vector<bool>& returnErrors) const = 0;
    /**
     * Runs the given statement on the given environment.
     */
    virtual void applyOn(const RamProgram& prog, RamEnvironment& env, RamData* data) const = 0;
};

}  // end of namespace souffle
