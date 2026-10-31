/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/**
 * @file The souffle executable
 */

#include "MainDriver.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace {

bool processArgs(souffle::Global& glb, int argc, char** argv) {
    /* have all to do with command line arguments in its own scope, as these are accessible through the global
     * configuration only */
    std::stringstream header;

    header << "============================================================================" << std::endl;
    header << "souffle -- A datalog engine." << std::endl;
    header << "Usage: souffle [OPTION] FILE." << std::endl;
    header << "----------------------------------------------------------------------------" << std::endl;
    header << "Options:" << std::endl;

    // command line options, the environment will be filled with the arguments passed to them, or
    // the empty string if they take none
    // main option, the datalog program itself, has an empty key
    const std::vector<souffle::MainOption> options = souffle::getMainOptions();
    glb.config().processArgs(argc, argv, header.str(), souffle::versionFooter(), options);

    return true;
}

int run(int argc, char** argv) {
    souffle::Global glb;

    processArgs(glb, argc, argv);

    return souffle::main(glb, argv[0]);
}

}  // namespace

int main(int argc, char** argv) {
#ifdef __EMSCRIPTEN__
    // In WebAssembly an uncaught C++ exception aborts the module without printing its
    // message, as a native build would; report it instead.
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
#else
    return run(argc, argv);
#endif
}
