/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstSrcLocation.cpp
 *
 * Structures to describe the location of AST nodes within input code.
 *
 ***********************************************************************/

#include "AstSrcLocation.h"
#include "Util.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

#include <libgen.h>
#include <unistd.h>

namespace souffle {

std::string AstSrcLocation::extloc() const {
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
            if (std::isspace(c)) {
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
        s << "unknown source location.";
    }
    return s.str();
}

}  // end of namespace souffle
