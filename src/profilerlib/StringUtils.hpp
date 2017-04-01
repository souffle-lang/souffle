/*
* Souffle - A Datalog Compiler
* Copyright (c) 2016, The Souffle Developers. All rights reserved
* Licensed under the Universal Permissive License v 1.0 as shown at:
* - https://opensource.org/licenses/UPL
* - <souffle root>/licenses/SOUFFLE-UPL.txt
*/

#pragma once

#include <string>
#include <vector>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>  // required for stat.h

#include <stdio.h> /* defines FILENAME_MAX */

#if defined(_WIN32) || defined(_WIN64) || defined(WINDOWS)
#include <direct.h>
#define GetCurrentDir _getcwd
#else

#include <unistd.h>

#define GetCurrentDir getcwd
#endif

#include "Table.hpp"

/*
 * A series of functions necessary throughout the code
 * Mostly string manipulation
 */

namespace Tools {
static const std::string arr[] = {"K", "M", "B", "t", "q", "Q", "s", "S", "o", "n", "d", "U"};
static const std::vector<std::string> abbreviations(arr, arr + sizeof(arr) / sizeof(arr[0]));

//    static typedef std::tuple<double, double, double, double, double, std::string, double, double, double>
//    rel_table_vals;

std::string formatNum(int precision, long amount);

std::string formatTime(double number);

std::vector<std::vector<std::string>> formatTable(Table table, int precision);

std::vector<std::string> split(std::string str, std::string split_reg);

std::vector<std::string> splitAtSemiColon(std::string str);

std::string trimWhitespace(std::string str);

inline bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

std::string getworkingdir();

std::string cleanString(std::string val);
std::string cleanJsonOut(std::string val);
std::string escapeQuotes(std::string val);
std::string cleanJsonOut(double val);

std::string stripWhitespace(std::string val);
}
