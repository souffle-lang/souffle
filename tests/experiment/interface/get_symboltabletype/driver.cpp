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
#include <typeinfo>
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

    // create instance of program "get_symboltabletype"
    if (SouffleProgram* prog = ProgramFactory::newInstance("get_symboltabletype")) {
        // load all input relations from current directory
        prog->loadAll(argv[1]);

        // run program
        prog->run();

        // this test is to check the structure of symbol table, and the type of the elements in the relation
        // "people"
        if (Relation* people = prog->getRelation("people")) {
            // Output the type of symbol table
            std::cout << people->getSignature() << "\n"
                      << "\n";

            // Output the symbol table
            std::cout << people->getSymbolTable() << "\n";

        } else {
            error("cannot find relation people");
        }

        prog->printAll();

        // free program
        delete prog;

    } else {
        error("cannot find program get_symboltabletype");
    }

    return 0;
}
