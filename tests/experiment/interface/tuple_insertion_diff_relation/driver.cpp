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
#include <csignal>
#include <iostream>
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
 * Signal handler
 */
void handler(int n) {
    std::cout << "handler invoked." << std::endl;
    exit(1);
}

/**
 * Main program
 */
int main(int argc, char** argv) {
    // create an instance of program "tuple_insertion_diff_relation"
    if (SouffleProgram* prog = ProgramFactory::newInstance("tuple_insertion_diff_relation")) {
        // get input relation "edge"
        if (Relation* edge = prog->getRelation("edge")) {
            std::vector<std::array<std::string, 2>> myData = {
                    {"A", "B"}, {"B", "C"}, {"C", "D"}, {"D", "E"}, {"E", "F"}, {"F", "A"}};
            for (auto input : myData) {
                tuple t(edge);
                t << input[0] << input[1];
                edge->insert(t);
            }

            if (Relation* line = prog->getRelation("line")) {
                std::vector<std::array<std::string, 2>> myData = {
                        {"1", "2"}, {"3", "4"}, {"5", "6"}, {"7", "8"}};
                for (auto input : myData) {
                    tuple t(line);
                    t << input[0] << input[1];
                    line->insert(t);
                }

                // set default signal  for SIGINT signal
                signal(SIGABRT, handler);

                // redirect stderr to a file called stderr
                freopen("stderr", "w", stderr);

                // this raises an assertion, which is caught by the handler function, which produces no stderr
                // output due to the redirection
                for (auto input : myData) {
                    tuple t(line);
                    t << input[0] << input[1];
                    edge->insert(t);
                }

                // run program
                prog->run();

                // print all relations to CSV files in current directory
                // NB: Defaul is current directory
                prog->printAll();

                // free program analysis
                delete prog;
            } else {
                error("cannot find relation line");
            }
        } else {
            error("cannot find relation edge");
        }
    } else {
        error("cannot find program tuple_insertion_diff_relation");
    }
}
