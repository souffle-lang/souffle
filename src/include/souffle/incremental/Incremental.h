/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Incremental.h
 *
 * Provenance interface for Souffle; works for compiler and interpreter
 *
 ***********************************************************************/

#pragma once

#include <csignal>
#include <iostream>
#include <regex>
#include <string>
#include <unistd.h>

// #include "CompiledOptions.h"
// #include "ProfileEvent.h"
#include "souffle/SouffleInterface.h"
// #include "WriteStreamCSV.h"

namespace souffle {

class Incremental {
public:
    SouffleProgram& prog;
    // CmdOptions& opt;

    Incremental(SouffleProgram& prog /*, CmdOptions& opt */) : prog(prog) /*, opt(opt) */, currentEpoch(0) {}

    /* Process a command, a return value of true indicates to continue, returning false indicates to break (if
     * the command is q/exit) */
    bool processCommand(std::string& commandLine) {
        std::vector<std::string> command = split(commandLine, ' ', 1);

        if (command.empty()) {
            return true;
        }

        if (command[0] == "insert") {
            std::pair<std::string, std::vector<std::string>> query;
            if (command.size() != 2) {
                printError("Usage: insert relation_name(\"<string element1>\", <number element2>, ...)\n");
                return true;
            }
            query = parseTuple(command[1]);
            insertTuple("_diff_plus_" + query.first, query.second);
        } else if (command[0] == "remove") {
            std::pair<std::string, std::vector<std::string>> query;
            if (command.size() != 2) {
                printError("Usage: remove relation_name(\"<string element1>\", <number element2>, ...)\n");
                return true;
            }
            query = parseTuple(command[1]);
            removeTuple("_diff_minus_" + query.first, query.second);
        } else if (command[0] == "commit") {
            // std::cout << "### BEGIN EPOCH " << currentEpoch << std::endl;
            currentEpoch++;
            commit();
        } else if (command[0] == "exit" || command[0] == "q") {
            return false;
        } else {
            printError(
                    "\n----------\n"
                    "Commands:\n"
                    "----------\n"
                    "insert <relation>(<element1>, <element2>, ...): Inserts a new tuple\n"
                    "remove <relation>(<element1>, <element2>, ...): Removes an existing tuple\n"
                    "commit: Re-runs the Datalog program incrementally to apply changes\n"
                    "exit: Exits this interface\n\n");
        }

        return true;
    }

    /* The main explain call */
    void startIncremental() {
        printPrompt("Incremental is invoked.\n");

        while (true) {
            printPrompt("Enter command > ");
            std::string line = getInput();

            // a return value of false indicates that an exit/q command has been processed
            if (processCommand(line) == false) {
                break;
            }

            // print all tuples - just for debugging for now
            // prog.printAll();
        }
    }

private:
    int currentEpoch;

    void addTuple(const std::string& relName, const std::vector<std::string>& tup) {
        auto rel = prog.getRelation(relName);
        if (rel == nullptr) {
            printError("Relation " + relName + " not found!\n");
            return;
        }

        if (tup.size() != rel->getArity()) {
            printError("Tuple not of the right arity!\n");
            return;
        }

        tuple newTuple(rel);

        for (size_t i = 0; i < rel->getArity(); i++) {
            if (*rel->getAttrType(i) == 's') {
                // remove quotation marks
                if (tup[i].size() >= 2 && tup[i][0] == '"' && tup[i][tup[i].size() - 1] == '"') {
                    newTuple << tup[i].substr(1, tup[i].size() - 2);
                }
            } else {
                newTuple << (std::stoi(tup[i]));
            }
        }

        rel->insert(newTuple);

        /*
        // epoch number
        auto epochRel = prog.getRelation("+current_epoch");
        tuple epochNumber(epochRel);
        std::cout << "inserting new epoch: " << currentEpoch << std::endl;
        epochNumber << currentEpoch;
        epochRel->insert(epochNumber);
        */

        // run program until fixpoint
        // prog.run();
    }

    void commit() {
        // if (opt.isProfiling()) {
        //     // std::cout << "profile filename: " << opt.getProfileName() << std::endl;
        //     ProfileEventSingleton::instance().setOutputFile(opt.getProfileName() +
        //     std::to_string(currentEpoch)); ProfileEventSingleton::instance().clear();
        // }

        // TODO: investigate what needs to be done for profiling

        std::vector<RamDomain> args;
        std::vector<RamDomain> ret;
        std::vector<bool> retErr;
        prog.executeSubroutine("update", args, ret);

        // if (opt.isProfiling()) {
        //     ProfileEventSingleton::instance().stopTimer();
        //     ProfileEventSingleton::instance().dump();
        // }
    }

    void insertTuple(const std::string& relName, std::vector<std::string> tup) {
        tup.push_back("0");
        // tup.push_back("0");
        tup.push_back("1");
        addTuple(relName, tup);
    }

    void removeTuple(const std::string& relName, std::vector<std::string> tup) {
        tup.push_back("0");
        // tup.push_back("1");
        tup.push_back("-1");
        addTuple(relName, tup);
    }

    /* Get input */
    std::string getInput() {
        std::string line;

        if (!getline(std::cin, line)) {
            // if EOF has been reached, quit
            line = "q";
        }

        return line;
    }

    /* Print a command prompt, disabled for non-terminal outputs */
    void printPrompt(const std::string& prompt) {
        if (!isatty(fileno(stdin))) {
            return;
        }
        std::cout << prompt;
    }

    /* Print any other information, disabled for non-terminal outputs */
    void printInfo(const std::string& info) {
        if (!isatty(fileno(stdin))) {
            return;
        }
        std::cout << info;
    }

    /* Print an error, such as a wrong command */
    void printError(const std::string& error) {
        std::cerr << error;
    }

    /**
     * Parse tuple, split into relation name and values
     * @param str The string to parse, should be something like "R(x1, x2, x3, ...)"
     */
    std::pair<std::string, std::vector<std::string>> parseTuple(const std::string& str) {
        std::string relName;
        std::vector<std::string> args;

        // regex for matching tuples
        // values matches numbers or strings enclosed in quotation marks
        std::regex relRegex(
                "([a-zA-Z0-9_.-]*)[[:blank:]]*\\(([[:blank:]]*([0-9]+|\"[^\"]*\")([[:blank:]]*,[[:blank:]]*(-"
                "?["
                "0-"
                "9]+|\"[^\"]*\"))*)?\\)",
                std::regex_constants::extended);
        std::smatch relMatch;

        // first check that format matches correctly
        // and extract relation name
        if (!std::regex_match(str, relMatch, relRegex) || relMatch.size() < 3) {
            return std::make_pair(relName, args);
        }

        // set relation name
        relName = relMatch[1];

        // extract each argument
        std::string argsList = relMatch[2];
        std::smatch argsMatcher;
        std::regex argRegex(R"(-?[0-9]+|"[^"]*")", std::regex_constants::extended);

        while (std::regex_search(argsList, argsMatcher, argRegex)) {
            // match the start of the arguments
            std::string currentArg = argsMatcher[0];
            args.push_back(currentArg);

            // use the rest of the arguments
            argsList = argsMatcher.suffix().str();
        }

        return std::make_pair(relName, args);
    }

    /** utility function to split a string */
    inline std::vector<std::string> split(const std::string& s, char delim, int times = -1) {
        std::vector<std::string> v;
        std::stringstream ss(s);
        std::string item;

        while ((times > 0 || times <= -1) && std::getline(ss, item, delim)) {
            v.push_back(item);
            times--;
        }

        if (ss.peek() != EOF) {
            std::string remainder;
            std::getline(ss, remainder);
            v.push_back(remainder);
        }

        return v;
    }
};

inline void startIncremental(SouffleProgram& prog /*, CmdOptions& opt */) {
    Incremental incr(prog /*, opt */);
    incr.startIncremental();
}

}  // end of namespace souffle
