/*
* Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#include "Cli.hpp"

void Cli::error() {
    std::cout << "souffle-profile -v | -h | <log-file> [ -c <command> | -j | -l ]\n";
    exit(1);
}

void Cli::parse() {
    std::vector<std::string> commands;
    std::string filename;
    bool alive = false;
    bool gui = false;

    if (args.size() <= 1) {
        error();
    }

    size_t i = 1;

    std::string arg = args.at(i++);
    if (arg.at(0) != '-') {
        filename = arg;
        if (args.size() > 2) {
            arg = args.at(i++);

            if (arg.compare("-c") == 0) {
                if (i < args.size()) {
                    // split at \s+ and save into commands
                    std::string& command_str = args.at(i++);
                    commands = Tools::split(command_str, " ");
                } else {
                    std::cout << "Parameters for option -c missing!\n";
                    error();
                }
            } else if (arg.compare("-l") == 0) {
                alive = true;
            } else if (arg.compare("-j") == 0) {
                gui = true;
            }
        }
    } else if (arg.compare("-h") == 0) {
        std::cout << "Souffle Profiler v3.0.1\n";
        std::cout << "usage: souffle-profile -v | -h | <log-file> [ -c <command> | -j | -l ]\n"
                  << "<log-file>     the selected log file to profile\n"
                  << "-c <command>   run the given command on the log file (run -c \"help\" for a list of "
                     "profiler commands)\n"
                  << "-j             generate a GUI(html/js) version of the profiler\n"
                  << "-l             run in live mode\n"
                  << "-v             print the profiler version\n"
                  << "-h             print this message" << std::endl;
        error();
    } else if (arg.compare("-v") == 0) {
        std::cout << "Souffle Profiler v3.0.1\n";
        error();
    } else if (arg.compare("-f") == 0) {
        std::cout << "Option -f has been phased out!\n";
        error();
    } else {
        std::cout << "Unknown argument " << arg << "\n";
        error();
    }

    if (filename.empty()) {
        error();
    }

    if (commands.size() > 0) {
        Tui(filename, alive, false).runCommand(commands);
    } else {
        if (gui) {
            Tui(filename, alive, true).outputJson();
        } else {
            Tui(filename, alive, false).runProf();
        }
    }
}
