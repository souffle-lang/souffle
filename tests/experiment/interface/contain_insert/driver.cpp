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
    // create an instance of program "contain_insert"
    if (SouffleProgram* prog = ProgramFactory::newInstance("contain_insert")) {
        // get input relation "edge"
        if (Relation* edge = prog->getRelation("edge")) {
            // load data into relation "edge" using "tuple"
            std::vector<std::array<std::string, 2>> myData = {
                    {"A", "B"}, {"B", "C"}, {"C", "D"}, {"D", "E"}, {"E", "F"}, {"F", "A"}};
            for (auto input : myData) {
                // create an empty tuple for relation "edge"
                tuple t(edge);

                // write elements into tuple
                t << input[0] << input[1];

                // insert tuple
                edge->insert(t);
            }

            std::tuple<std::string, std::string> t1("F", "A");

            std::tuple<std::string, std::string> t2("X", "Z");

            // Check if "edge" contain a tuple {"F", "A"} using "tuple" from std lib
            if (prog->contains(t1, edge)) {
                // If {"F", "A"} exists, insert {"X", "Z"} using "tuple" from std lib
                prog->insert(t2, edge);
            }

            tuple t3(edge);
            t3 << "X"
               << "Z";

            tuple t4(edge);
            t4 << "J"
               << "K";

            // Check if "edge" contain a tuple {"X", "Z"} using "tuple" from Souffle
            if (edge->contains(t3)) {
                // If {"F", "A"} exists, insert {"J", "K"} using "tuple" from Souffle
                edge->insert(t4);
            }

            // run program
            prog->run();

            // print all relations to CSV files in current directory
            // NB: Defaul is current directory
            prog->printAll();

            // free program analysis
            delete prog;

        } else {
            error("cannot find relation edge");
        }
    } else {
        error("cannot find program contain_insert");
    }
}
