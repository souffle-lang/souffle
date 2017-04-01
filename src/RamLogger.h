/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamLogger.h
 *
 * A logger is the utility utilized by RAM programs to create logs and
 * traces.
 *
 ***********************************************************************/

#pragma once

#include "ParallelUtils.h"

#include <chrono>
#include <iostream>

namespace souffle {

/**
 * Obtains a reference to the lock synchronizing output operations.
 */
inline Lock& getOutputLock() {
    static Lock output_lock;
    return output_lock;
}

/**
 * The class utilized to times for the souffle profiling tool. This class
 * is utilized by both -- the interpreted and compiled version -- to conduct
 * the corresponding measurements.
 *
 * To far, only execution times are logged. More events, e.g. the number of
 * processed tuples may be added in the future.
 */
class RamLogger {
    // the type of clock to be utilized by this class
    typedef std::chrono::steady_clock clock;
    typedef clock::time_point time;

    // a label to be printed when reporting the execution time
    const char* label;

    // the start time
    time start;

    // an output stream to report to
    std::ostream& out;

public:
    RamLogger(const char* label, std::ostream& out = std::cout) : label(label), out(out) {
        start = clock::now();
    }

    ~RamLogger() {
        auto duration = clock::now() - start;

        auto leas = getOutputLock().acquire();
        (void)leas;  // avoid warning
        out << label << std::chrono::duration_cast<std::chrono::duration<double>>(duration).count()
            << std::endl;
    }
};

}  // end of namespace souffle
