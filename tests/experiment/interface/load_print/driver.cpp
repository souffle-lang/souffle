/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file driver.cpp
 *
 * Driver program for invoking a Souffle program using the OO-interface
 *
 ***********************************************************************/

#include "souffle/SouffleInterface.h"
#include <array>
#include <string>
#include <vector>

using namespace souffle;

/**
 * Error handler
 */
void error(std::string txt) {
    std::cerr << "error: " << txt << "\n";
    exit(1);
}

/**
 * Main program
 */
int main(int argc, char** argv) {
    // check number of arguments
    if (argc != 2) {
        error("wrong number of arguments!");
    }

    // create instance of program "load_print"
    if (SouffleProgram* prog = ProgramFactory::newInstance("load_print")) {
        // load all input relations from current directory
        prog->loadAll(argv[1]);

        // run program
        prog->run();

        // print all relations to CSV files in current directory
        prog->printAll();

        // free program
        delete prog;

    } else {
        error("cannot find program load_print");
    }
}
