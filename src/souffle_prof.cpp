/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file souffle_prof.cpp
 *
 * Main program of souffle's profiler
 *
 ***********************************************************************/

#include "souffle/profile/Cli.h"

#include <iostream>

int main(int argc, char* argv[]) {
    souffle::profile::Cli cli_obj(argc, argv);
    cli_obj.parse();
    std::cout << "\n";
    return 0;
}
