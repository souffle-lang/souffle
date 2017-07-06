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
#include <iostream>
#include <string>
#include <assert.h>
#include <signal.h>

namespace souffle {

/**
 * Class SignalHandler captures signals
 * and reports the context where the signal occurs.
 * The signal handler is implemented as a singleton.
 */
class SignalHandler {
private:
    // signal context information
    std::atomic<const char*> msg;

    /**
     * Signal handler for various types of signals.
     */
    static void handler(int signal) {
        const char* msg = instance()->msg;
        std::string error;
        switch (signal) {
            case SIGINT:
                error = "Interrupt";
                break;
            case SIGFPE:
                error = "Floating-point arithmetic exception";
                break;
            case SIGSEGV:
                error = "Segmentation violation";
                break;
            default:
                error = "Unknown";
                break;
        }
        if (msg != nullptr) {
            std::cerr << error << " signal in rule:\n" << msg << std::endl;
        } else {
            std::cerr << error << " signal." << std::endl;
        }
        exit(1);
    }

    SignalHandler() : msg(nullptr) {
        // register signals
        signal(SIGFPE, handler);   // floating point exception
        signal(SIGINT, handler);   // user interrupts
        signal(SIGSEGV, handler);  // memory issues
    }

public:
    // get singleton
    static SignalHandler* instance() {
        static SignalHandler singleton;
        return &singleton;
    }

    // set signal message
    void setMsg(const char* m) {
        msg = m;
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
};

}  // namespace souffle
