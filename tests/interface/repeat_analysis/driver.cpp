/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019 The Souffle Developers. All Rights reserved
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
#include <memory>
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

void printSource2sink(std::unique_ptr<SouffleProgram>& prog) {
    Relation* source2sink = prog->getRelation("source2sink");
    for (tuple tuple : *source2sink) {
        std::string field;
        std::string field2;
        tuple >> field;
        tuple >> field2;
        std::cout << field << "-" << field2 << std::endl;
    }
}
/**
 * Main program
 */
int main(int argc, char** argv) {
    std::unique_ptr<SouffleProgram> prog(ProgramFactory::newInstance("repeat_analysis"));
    if (prog == nullptr) {
        error("failed to create souffle program");
    }
    // load the facts
    prog->loadAll();
    // run the program
    prog->run();
    // there should be no source2sink
    std::cout << "source2sink - run 1" << std::endl;
    printSource2sink(prog);

    // add 2  sources
    Relation* source = prog->getRelation("source");
    tuple sourceB(source);
    sourceB << "B";
    source->insert(sourceB);
    tuple sourceC(source);
    sourceC << "C";
    source->insert(sourceC);

    // add a sink
    Relation* sink = prog->getRelation("sink");
    tuple sinkFact(sink);
    sinkFact << "F";
    sink->insert(sinkFact);

    // run the program again
    prog->run();
    // there should be 2 source2sink
    std::cout << "source2sink - run 2" << std::endl;
    printSource2sink(prog);

    source = prog->getRelation("source");
    source->purge();
    prog->purgeInternalRelations();
    prog->purgeOutputRelations();

    // add a different source
    source = prog->getRelation("source");
    tuple sourceD(source);
    sourceD << "D";
    source->insert(sourceD);

    // run the program a final time
    prog->run();
    std::cout << "source2sink - run 3" << std::endl;
    printSource2sink(prog);
}
