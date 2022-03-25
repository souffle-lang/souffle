/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SrcLocation.cpp
 *
 * Structures to describe the location of AST nodes within input code.
 *
 ***********************************************************************/

#ifdef _MSC_VER
// do not define min and max otherwise cannot use std::min std::max
#define NOMINMAX
#endif

#include "parser/SrcLocation.h"
#include "souffle/utility/FileUtil.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace souffle {

std::string getCurrentFilename(const std::vector<std::string>& filenames) {
    if (filenames.empty()) {
        return "";
    }

    std::string path = ".";
    for (const std::string& filename : filenames) {
        if (!filename.empty() && isAbsolute(filename)) {
            path = dirName(filename);
        } else if (existFile(path + pathSeparator + filename)) {
            path = dirName(path + pathSeparator + filename);
        } else if (existFile(filename)) {
            path = dirName(filename);
        } else {
            path = ".";
        }
    }

    return path + pathSeparator + baseName(filenames.back());
}

bool SrcLocation::operator<(const SrcLocation& other) const {
    // Translate filename stack into current files
    std::string filename = getCurrentFilename(filenames);
    std::string otherFilename = getCurrentFilename(other.filenames);

    if (filename < otherFilename) {
        return true;
    }

    if (filename > otherFilename) {
        return false;
    }
    if (start < other.start) {
        return true;
    }
    if (start > other.start) {
        return false;
    }
    if (end < other.end) {
        return true;
    }
    return false;
}

void SrcLocation::setFilename(std::string filename) {
    makePreferred(filename);
    if (filenames.empty()) {
        filenames.emplace_back(filename);
        return;
    }
    if (filenames.back() == filename) {
        return;
    }
    if (filenames.size() > 1 && filenames.at(filenames.size() - 2) == filename) {
        filenames.pop_back();
        return;
    }
    filenames.emplace_back(filename);
}

std::string SrcLocation::extloc() const {
    std::string filename = getCurrentFilename(filenames);
    std::ifstream in(filename);
    std::stringstream s;
    if (in.is_open()) {
        s << "file " << baseName(filename) << " at line " << start.line << "\n";
        for (int i = 0; i < start.line - 1; ++i) {
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        int c;
        int lineLen = 0;
        int offsetColumn = start.column;
        bool prevWhitespace = false;
        bool afterFirstNonSpace = false;
        while ((c = in.get()) != '\n' && c != EOF) {
            s << (char)c;
            lineLen++;

            // Offset column to account for C preprocessor having reduced
            // consecutive non-leading whitespace chars to a single space.
            if (std::isspace(c) != 0) {
                if (afterFirstNonSpace && prevWhitespace && offsetColumn >= lineLen) {
                    offsetColumn++;
                }
                prevWhitespace = true;
            } else {
                prevWhitespace = false;
                afterFirstNonSpace = true;
            }
        }
        lineLen++;  // Add new line
        in.close();
        s << "\n";
        for (int i = 1; i <= lineLen; i++) {
            char ch = (i == offsetColumn) ? '^' : '-';
            s << ch;
        }
        in.close();
    } else {
        s << filename << ":" << start.line << ":" << start.column;
    }
    return s.str();
}

void SrcLocation::print(std::ostream& out) const {
    out << getCurrentFilename(filenames) << " [" << start << "-" << end << "]";
}
}  // end of namespace souffle
