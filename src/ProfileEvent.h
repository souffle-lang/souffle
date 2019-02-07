/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ProfileEvent.h
 *
 * Declares classes for profile events
 *
 ***********************************************************************/

#pragma once

#include "EventProcessor.h"
#include "ProfileDatabase.h"
#include "Util.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <sys/resource.h>
#include <sys/time.h>

namespace souffle {

/**
 * Profile Event Singleton
 */
class ProfileEventSingleton {
    /** profile database */
    profile::ProfileDatabase database;
    std::string filename{""};

    ProfileEventSingleton() = default;

public:
    ~ProfileEventSingleton() {
        stopTimer();
        ProfileEventSingleton::instance().dump();
    }

    /** get instance */
    static ProfileEventSingleton& instance() {
        static ProfileEventSingleton singleton;
        return singleton;
    }

    /** create config record */
    void makeConfigRecord(const std::string& key, const std::string& value) {
        profile::EventProcessorSingleton::instance().process(database, "@config", key.c_str(), value.c_str());
    }

    /** create stratum record */
    void makeStratumRecord(size_t index, const std::string& type, const std::string& relName,
            const std::string& key, const std::string& value) {
        std::stringstream ss;
        ss << "@text;stratum;" << index << ';' << type << ';' << relName << ';' << key;
        profile::EventProcessorSingleton::instance().process(database, ss.str().c_str(), value.c_str());
    }

    /** create time event */
    void makeTimeEvent(const std::string& txt) {
        profile::EventProcessorSingleton::instance().process(
                database, txt.c_str(), std::chrono::duration_cast<microseconds>(now().time_since_epoch()));
    }

    /** create an event for recording start and end times */
    void makeTimingEvent(const std::string& txt, time_point start, time_point end, size_t startMaxRSS,
            size_t endMaxRSS, size_t size, size_t iteration) {
        microseconds start_ms = std::chrono::duration_cast<microseconds>(start.time_since_epoch());
        microseconds end_ms = std::chrono::duration_cast<microseconds>(end.time_since_epoch());
        profile::EventProcessorSingleton::instance().process(
                database, txt.c_str(), start_ms, end_ms, startMaxRSS, endMaxRSS, size, iteration);
    }

    /** create quantity event */
    void makeQuantityEvent(const std::string& txt, size_t number, int iteration) {
        profile::EventProcessorSingleton::instance().process(database, txt.c_str(), number, iteration);
    }

    /** create utilisation event */
    void makeUtilisationEvent(const std::string& txt) {
        /* current time */
        microseconds time = std::chrono::duration_cast<microseconds>(now().time_since_epoch());
        /* system CPU time used */
        struct rusage ru {};
        getrusage(RUSAGE_SELF, &ru);
        /* system CPU time used */
        uint64_t systemTime = ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
        /* user CPU time used */
        uint64_t userTime = ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec;
        /* Maximum resident set size (kb) */
        size_t maxRSS = ru.ru_maxrss;

        profile::EventProcessorSingleton::instance().process(
                database, txt.c_str(), time, systemTime, userTime, maxRSS);
    }

    void setOutputFile(std::string filename) {
        this->filename = filename;
    }
    /** Dump all events */
    void dump() {
        if (!filename.empty()) {
            std::ofstream os(filename);
            if (!os.is_open()) {
                std::cerr << "Cannot open profile log file <" + filename + ">";
            } else {
                database.print(os);
            }
        }
    }

    /** Start timer */
    void startTimer() {
        timer.start();
    }

    /** Stop timer */
    void stopTimer() {
        timer.stop();
    }

    void resetTimerInterval(uint32_t interval = 1) {
        timer.resetTimerInterval(interval);
    }
    const profile::ProfileDatabase& getDB() const {
        return database;
    }

    void setDBFromFile(const std::string& filename) {
        database = profile::ProfileDatabase(filename);
    }

private:
    /**  Profile Timer */
    class ProfileTimer {
    private:
        /** time interval between per utilisation read */
        uint32_t t;

        /** timer is running */
        std::atomic<bool> running{false};

        /** thread timer runs on */
        std::thread th;

        std::condition_variable conditionVariable;
        std::mutex timerMutex;

        /** number of utilisation events */
        std::atomic<uint32_t> runCount{0};

        /** run method for thread th */
        void run() {
            ProfileEventSingleton::instance().makeUtilisationEvent("@utilisation");
            ++runCount;
            if (runCount % 128 == 0) {
                increaseInterval();
            }
        }

        uint32_t getInterval() {
            return t;
        }

        /** Increase value of time interval by factor of 2 */
        void increaseInterval() {
            // Don't increase time interval past 60 seconds
            if (t < 60000) {
                t = t * 2;
            }
        }

    public:
        /*
         *  @param interval the size of the timing interval in milliseconds
         */
        ProfileTimer(uint32_t interval = 10) : t(interval) {}

        /** start timer on the thread th */
        void start() {
            if (running) {
                return;
            }
            running = true;

            th = std::thread([this]() {
                while (running) {
                    run();
                    std::unique_lock<std::mutex> lock(timerMutex);
                    conditionVariable.wait_for(lock, std::chrono::milliseconds(getInterval()));
                }
            });
        }

        /** stop timer on the thread th */
        void stop() {
            running = false;
            conditionVariable.notify_all();
            if (th.joinable()) {
                th.join();
            }
        }

        /** Reset timer interval.
         *
         *  The timer interval increases as the program executes. Resetting the interval is useful to
         *  ensure that detailed usage information is gathered even in long running programs, if desired.
         *
         *  @param interval the size of the timing interval in milliseconds
         */
        void resetTimerInterval(uint32_t interval = 10) {
            t = interval;
            runCount = 0;
            conditionVariable.notify_all();
        }
    };

    ProfileTimer timer;
};

}  // namespace souffle
