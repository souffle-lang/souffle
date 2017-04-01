/*
* Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include "StringUtils.hpp"

/*
 * Class containing a copy of the gui_src directory (apart from testtabledata) packaged into one html file
 * so that a data variable can be inserted in the middle of the two strings and written to a file.
 *
 * TODO: test reading directly from gui_src files
 * TODO: after reading from gui_src a js/css minification process could be added to reduce file size
 *  - not necessary at this point as the packaged file is ~100kb
 */
class html_string {
private:
    std::string first_half;
    std::string second_half;

public:
    html_string() {
        std::string current_dir = __FILE__;
        std::string make_directory = MAKEDIR;  // Variable set as flag in src/Makefile.am

        std::vector<std::string> profiler_dir = Tools::split(current_dir, "/");

        if (profiler_dir.size() == 0) {
            std::cerr << "Error loading source profiler directory, ensure makefile contains the lines:\n"
                         "DIR := ${CURDIR}"
                         "souffle_profile_CXXFLAGS = $(souffle_CPPFLAGS) -DMAKEDIR='\"$(DIR)\"'";
        }

        std::string gui_directory = make_directory;

        for (int i = 0; i < profiler_dir.size() - 1; ++i) {
            gui_directory += "/" + profiler_dir.at(i);
        }

        std::string GUI_DIR = "gui_src";  // TODO: change from hard coded string

        gui_directory += "/" + GUI_DIR + "/";

        std::cout << gui_directory << std::endl;

        bool adding_to_first = true;

        std::ifstream infile(gui_directory + "main.html");
        std::string line;
        std::string output;
        while (std::getline(infile, line)) {
            output = "";
            if (line.find("<link") == 0) {
                std::vector<std::string> src = Tools::split(line, "href=\"");
                if (src.size() > 1) {
                    output = "<style>\n";
                    std::ifstream infile2(gui_directory + Tools::split(src.at(1), "\"").at(0));
                    while (std::getline(infile2, line)) {
                        output += line + "\n";
                    }
                    output += "\n</style>\n";
                    std::cout << output;
                } else {
                    output = line;
                }
            } else if (line.find("<script") == 0) {
                std::vector<std::string> src = Tools::split(line, "src=\"");
                if (src.size() > 1) {
                    output = "<script>\n";
                    std::string filename = Tools::split(src.at(1), "\"").at(0);
                    if (filename == "testtabledata.js") {  // TODO: another hard coded string
                        this->first_half += output;
                        adding_to_first = false;
                        output = "\n</script>\n";
                    } else {
                        std::ifstream infile2(gui_directory + filename);
                        while (std::getline(infile2, line)) {
                            output += line + "\n";
                        }
                        output += "\n</script>\n";
                        std::cout << output;
                    }
                } else {
                    output = line;
                }
            } else {
                output = line;
            }
            if (output.empty()) continue;

            if (adding_to_first)
                this->first_half += output;
            else
                this->second_half += output;
        }
    }

    inline std::string get_first_half() {
        return this->first_half;
    }

    inline std::string get_second_half() {
        return this->second_half;
    }
};