/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2017, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Explain.h
 *
 * Provenance interface for Souffle; works for compiler and interpreter
 *
 ***********************************************************************/

#pragma once

#include "ExplainProvenanceSLD.h"

#include <csignal>
#include <iostream>
#include <regex>
#include <string>
#include <ncurses.h>

#include "SouffleInterface.h"
#include "WriteStreamCSV.h"

#define MAX_TREE_HEIGHT 500
#define MAX_TREE_WIDTH 500

namespace souffle {

class Explain {
private:
    ExplainProvenance& prov;

    bool ncurses;
    WINDOW* treePad = nullptr;
    WINDOW* queryWindow = nullptr;
    int maxx = 0, maxy = 0;

    int depthLimit;

    /** output stream */
    std::ostream* output;

    /** flag for outputting in JSON format */
    bool json;

    // parse relation, split into relation name and values
    std::pair<std::string, std::vector<std::string>> parseTuple(const std::string& str) {
        std::string relName;
        std::vector<std::string> args;

        // regex for matching tuples
        // values matches numbers or strings enclosed in quotation marks
        std::regex relRegex(
                "([a-zA-Z0-9_.-]*)[[:blank:]]*\\(([[:blank:]]*([0-9]+|\"[^\"]*\")([[:blank:]]*,[[:blank:]]*(["
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
        std::regex argRegex(R"([0-9]+|"[^"]*")", std::regex_constants::extended);

        while (std::regex_search(argsList, argsMatcher, argRegex)) {
            // match the start of the arguments
            std::string currentArg = argsMatcher[0];
            args.push_back(currentArg);

            // use the rest of the arguments
            argsList = argsMatcher.suffix().str();
        }

        return std::make_pair(relName, args);
    }

    // initialise ncurses window
    WINDOW* makeQueryWindow() {
        WINDOW* w = newwin(3, maxx, maxy - 2, 0);
        wrefresh(w);
        return w;
    }

    // print provenance tree
    void printTree(std::unique_ptr<TreeNode> tree) {
        if (tree) {
            if (!json) {
                tree->place(0, 0);
                ScreenBuffer screenBuffer(tree->getWidth(), tree->getHeight());
                tree->render(screenBuffer);
                if (ncurses && !output) {
                    wprintw(treePad, screenBuffer.getString().c_str());
                } else {
                    if (output) {
                        *output << screenBuffer.getString();
                    } else {
                        std::cout << screenBuffer.getString();
                    }
                }
            } else {
                if (!output) {
                    std::cout << "{ \"proof\":\n";
                    tree->printJSON(std::cout, 1);
                    std::cout << ",";
                    prov.printRulesJSON(std::cout);
                    std::cout << "}\n";
                } else {
                    *output << "{ \"proof\":\n";
                    tree->printJSON(*output, 1);
                    *output << ",";
                    prov.printRulesJSON(*output);
                    *output << "}\n";
                }
            }
        }
    }

    // print string
    void printStr(const std::string& s) {
        if (ncurses && !output) {
            wprintw(treePad, s.c_str());
        } else {
            if (output) {
                *output << s;
            } else {
                std::cout << s;
            }
        }
    }

    // allow scrolling of provenance tree
    void scrollTree(int maxx, int maxy) {
        int x = 0;
        int y = 0;

        while (true) {
            int ch = wgetch(treePad);

            if (ch == KEY_LEFT) {
                if (x > 2) x -= 3;
            } else if (ch == KEY_RIGHT) {
                if (x < MAX_TREE_WIDTH - 3) x += 3;
            } else if (ch == KEY_UP) {
                if (y > 0) y -= 1;
            } else if (ch == KEY_DOWN) {
                if (y < MAX_TREE_HEIGHT - 1) y += 1;
            } else {
                ungetch(ch);
                break;
            }

            prefresh(treePad, y, x, 0, 0, maxy - 3, maxx - 1);
        }
    }

    // initialise ncurses window
    void initialiseWindow() {
        initscr();

        // get size of window
        getmaxyx(stdscr, maxy, maxx);

        // create windows for query and tree display
        queryWindow = makeQueryWindow();
        treePad = newpad(MAX_TREE_HEIGHT, MAX_TREE_WIDTH);

        keypad(treePad, true);
    }

public:
    Explain(ExplainProvenance& p, bool ncurses, int d = 4)
            : prov(p), ncurses(ncurses), depthLimit(d), output(nullptr), json(false) {}
    ~Explain() {
        delete output;
    }

    void explain() {
        if (ncurses && !output) {
            initialiseWindow();
            std::signal(SIGWINCH, nullptr);
        }

        // process commands
        char buf[100];
        std::string line;

        while (true) {
            if (ncurses && !output) {
                // reset command line on each loop
                werase(queryWindow);
                wrefresh(queryWindow);
                mvwprintw(queryWindow, 1, 0, "Enter command > ");
                curs_set(1);
                echo();

                // get next command
                wgetnstr(queryWindow, buf, 100);
                noecho();
                curs_set(0);
                line = buf;

                // reset tree display on each loop
                werase(treePad);
                prefresh(treePad, 0, 0, 0, 0, maxy - 3, maxx - 1);
            } else {
                std::cout << "Enter command > ";
                if (!getline(std::cin, line)) {
                    printStr("Exiting explain\n");
                    break;
                }
            }

            std::vector<std::string> command = split(line, ' ', 1);

            if (command.empty()) {
                continue;
            }

            if (command[0] == "setdepth") {
                if (command.size() != 2) {
                    printStr("Usage: setdepth <depth>\n");
                    continue;
                }
                try {
                    depthLimit = std::stoi(command[1]);
                } catch (std::exception& e) {
                    printStr("<" + command[1] + "> is not a valid depth\n");
                    continue;
                }
                printStr("Depth is now " + std::to_string(depthLimit) + "\n");
            } else if (command[0] == "explain") {
                std::pair<std::string, std::vector<std::string>> query;
                if (command.size() == 2) {
                    query = parseTuple(command[1]);
                } else {
                    printStr("Usage: explain relation_name(\"<string element1>\", <number element2>, ...)\n");
                    continue;
                }
                std::unique_ptr<TreeNode> t = prov.explain(query.first, query.second, depthLimit);
                printTree(std::move(t));
            } else if (command[0] == "subproof") {
                std::pair<std::string, std::vector<std::string>> query;
                int label = -1;
                if (command.size() > 1) {
                    query = parseTuple(command[1]);
                    label = std::stoi(query.second[0]);
                } else {
                    printStr("Usage: subproof relation_name(<label>)\n");
                    continue;
                }
                std::unique_ptr<TreeNode> t = prov.explainSubproof(query.first, label, depthLimit);
                printTree(std::move(t));
            } else if (command[0] == "rule" && command.size() == 2) {
                auto query = split(command[1], ' ');
                if (query.size() != 2) {
                    printStr("Usage: rule <relation name> <rule number>\n");
                    continue;
                }
                try {
                    printStr(prov.getRule(query[0], std::stoi(query[1])) + "\n");
                } catch (std::exception& e) {
                    printStr("Usage: rule <relation name> <rule number>\n");
                    continue;
                }
            } else if (command[0] == "measure") {
                try {
                    printStr(prov.measureRelation(command[1]));
                } catch (std::exception& e) {
                    printStr("Usage: printrel <relation name>\n");
                    continue;
                }
            } else if (command[0] == "output") {
                if (command.size() == 2) {
                    output = new std::ofstream(command[1]);
                } else if (command.size() == 1) {
                    delete output;
                    output = nullptr;
                } else {
                    printStr("Usage: output  [<filename>]\n");
                }
            } else if (command[0] == "format") {
                if (command.size() == 2 && command[1] == "json") {
                    json = true;
                } else if (command.size() == 2 && command[1] == "proof") {
                    json = false;
                } else {
                    printStr("Usage: format <json|proof>\n");
                }
            } else if (command[0] == "exit" || command[0] == "q" || command[0] == "quit") {
                printStr("Exiting explain\n");
                break;
            } else {
                printStr(
                        "\n----------\n"
                        "Commands:\n"
                        "----------\n"
                        "setdepth <depth>: Set a limit for printed derivation tree height\n"
                        "explain <relation>(<element1>, <element2>, ...): Prints derivation tree\n"
                        "subproof <relation>(<label>): Prints derivation tree for a subproof, label is "
                        "generated if a derivation tree exceeds height limit\n"
                        "rule <relation name> <rule number>: Prints a rule\n"
                        "output <filename>: Write output into a file/disable output\n"
                        "format <json|proof>: switch format between json and proof-trees\n"
                        "exit: Exits this interface\n\n");
            }

            // refresh treePad and allow scrolling
            if (ncurses && !output) {
                prefresh(treePad, 0, 0, 0, 0, maxy - 3, maxx - 1);
                scrollTree(maxx, maxy);
            }
        }
        if (ncurses && !output) {
            endwin();
        }
    }
};

inline void explain(SouffleProgram& prog, bool sld = true, bool ncurses = false) {
    std::cout << "Explain is invoked.\n";

    ExplainProvenanceSLD prov(prog);

    Explain exp(prov, ncurses);
    exp.explain();
}

}  // end of namespace souffle
