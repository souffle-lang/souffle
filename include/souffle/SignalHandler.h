/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SignalHandler.h
 *
 * A signal handler for Souffle's interpreter and compiler.
 *
 ***********************************************************************/

#pragma once

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>

namespace souffle {

/**
 * Class SignalHandler captures signals
 * and reports the context where the signal occurs.
 * The signal handler is implemented as a singleton.
 */
class SignalHandler {
public:
    // get singleton
    static SignalHandler* instance() {
        static SignalHandler singleton;
        return &singleton;
    }

    // Enable logging
    void enableLogging() {
        logMessages = true;
    }
    // set signal message
    void setMsg(const char* m) {
        if (logMessages && m != nullptr) {
            static std::mutex outputMutex;
            static bool sameLine = false;
            std::lock_guard<std::mutex> guard(outputMutex);
            if (msg != nullptr && strcmp(m, msg) == 0) {
                std::cout << ".";
                sameLine = true;
            } else {
                if (sameLine) {
                    sameLine = false;
                    std::cout << std::endl;
                }
                std::string outputMessage(m);
                for (char& c : outputMessage) {
                    if (c == '\n' || c == '\t') {
                        c = ' ';
                    }
                }
                std::cout << "Starting work on " << outputMessage << std::endl;
            }
        }
        msg = m;
    }

    /***
     * set signal handlers
     */
    void set() {
        if (!isSet && std::getenv("SOUFFLE_ALLOW_SIGNALS") == nullptr) {
            // register signals
            // floating point exception
            if ((prevFpeHandler = signal(SIGFPE, handler)) == SIG_ERR) {
                perror("Failed to set SIGFPE signal handler.");
                exit(1);
            }
            // user interrupts
            if ((prevIntHandler = signal(SIGINT, handler)) == SIG_ERR) {
                perror("Failed to set SIGINT signal handler.");
                exit(1);
            }
            // memory issues
            if ((prevSegVHandler = signal(SIGSEGV, handler)) == SIG_ERR) {
                perror("Failed to set SIGSEGV signal handler.");
                exit(1);
            }
            isSet = true;
        }
    }

    /***
     * reset signal handlers
     */
    void reset() {
        if (isSet) {
            // reset floating point exception
            if (signal(SIGFPE, prevFpeHandler) == SIG_ERR) {
                perror("Failed to reset SIGFPE signal handler.");
                exit(1);
            }
            // user interrupts
            if (signal(SIGINT, prevIntHandler) == SIG_ERR) {
                perror("Failed to reset SIGINT signal handler.");
                exit(1);
            }
            // memory issues
            if (signal(SIGSEGV, prevSegVHandler) == SIG_ERR) {
                perror("Failed to reset SIGSEGV signal handler.");
                exit(1);
            }
            isSet = false;
        }
    }

    /***
     * error handling routine that prints the rule context.
     */

    void error(const std::string& error) {
        if (msg != nullptr) {
            std::cerr << error << " in rule:\n" << msg << std::endl;
        } else {
            std::cerr << error << std::endl;
        }
        exit(1);
    }

private:
    // signal context information
    std::atomic<const char*> msg;

    // state of signal handler
    bool isSet = false;

    bool logMessages = false;

    // previous signal handler routines
    void (*prevFpeHandler)(int) = nullptr;
    void (*prevIntHandler)(int) = nullptr;
    void (*prevSegVHandler)(int) = nullptr;

    /**
     * Signal handler for various types of signals.
     */
    static void handler(int signal) {
        const char* msg = instance()->msg;
        std::string error;
        switch (signal) {
            case SIGINT: error = "Interrupt"; break;
            case SIGFPE: error = "Floating-point arithmetic exception"; break;
            case SIGSEGV: error = "Segmentation violation"; break;
            default: error = "Unknown"; break;
        }
        if (msg != nullptr) {
            std::cerr << error << " signal in rule:\n" << msg << std::endl;
        } else {
            std::cerr << error << " signal." << std::endl;
        }
        exit(1);
    }

    SignalHandler() : msg(nullptr) {}
};

}  // namespace souffle
