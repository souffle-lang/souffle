/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file driver.cpp
 *
 * Driver program to check whether signal handlers are restored
 * after invocation of Souffle's program.
 *
 ***********************************************************************/

#include "souffle/SouffleInterface.h"
#include <array>
#include <csignal>
#include <string>
#include <vector>

using namespace souffle;

/**
 * Signal handler
 */
void handler(int n) {
    std::cerr << "handler invoked." << std::endl;
    exit(1);
}

/**
 * Main program
 */
int main(int argc, char** argv) {
    // set default signal handler for SIGINT signal
    signal(SIGINT, handler);

    // create instance of program "signal_error"
    if (SouffleProgram* prog = ProgramFactory::newInstance("signal_error")) {
        // run program
        prog->run();

        // free program
        delete prog;

        // raise signal SIGINT to check that original signal handler is restored
        raise(SIGINT);

    } else {
        std::cerr << "cannot find program signal_error" << std::endl;
        return 1;
    }

    return 0;
}
