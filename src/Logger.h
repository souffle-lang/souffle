/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Logger.h
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
class Logger {
    // the type of clock to be utilized by this class
    typedef std::chrono::steady_clock clock;
    typedef clock::time_point time;

    // a label to be printed when reporting the execution time
    const char* label;

    // the start time
    time start;

    // an output stream to report to
    std::ostream& out;

    // extension of log file determining message format
    const std::string ext;

public:
    Logger(const char* label, std::ostream& out = std::cout, const std::string ext = "")
            : label(label), out(out), ext(ext) {
        start = clock::now();
    }

    ~Logger() {
        auto duration = clock::now() - start;

        auto lease = getOutputLock().acquire();
        (void)lease;  // avoid warning

        out << label << std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();

        if (ext == "json") {
            out << "}";
            if (std::string(label).find("@runtime") != std::string::npos) {
                out << "\n]";
            } else {
                out << ",";
            }
        }

        out << "\n";
    }
};

}  // end of namespace souffle
