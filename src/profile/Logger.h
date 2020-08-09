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

#include "ProfileEvent.h"
#include "utility/MiscUtil.h"
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <sys/resource.h>

namespace souffle {

/**
 * The class utilized to times for the souffle profiling tool. This class
 * is utilized by both -- the interpreted and compiled version -- to conduct
 * the corresponding measurements.
 *
 * To far, only execution times are logged. More events, e.g. the number of
 * processed tuples may be added in the future.
 */
class Logger {
public:
    Logger(std::string label, size_t iteration) : Logger(label, iteration, []() { return 0; }) {}

    Logger(std::string label, size_t iteration, std::function<size_t()> size)
            : label(std::move(label)), start(now()), iteration(iteration), size(size), preSize(size()) {
        struct rusage ru {};
        getrusage(RUSAGE_SELF, &ru);
        startMaxRSS = ru.ru_maxrss;
        // Assume that if we are logging the progress of an event then we care about usage during that time.
        ProfileEventSingleton::instance().resetTimerInterval();
    }

    ~Logger() {
        struct rusage ru {};
        getrusage(RUSAGE_SELF, &ru);
        size_t endMaxRSS = ru.ru_maxrss;
        ProfileEventSingleton::instance().makeTimingEvent(
                label, start, now(), startMaxRSS, endMaxRSS, size() - preSize, iteration);
    }

private:
    std::string label;
    time_point start;
    size_t startMaxRSS;
    size_t iteration;
    std::function<size_t()> size;
    size_t preSize;
};
}  // end of namespace souffle
