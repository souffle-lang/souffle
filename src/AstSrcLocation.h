/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstSrcLocation.h
 *
 * Structures to describe the location of AST nodes within input code.
 *
 ***********************************************************************/

#pragma once

#include <ostream>
#include <string>

namespace souffle {

/** A class describing a range in an input file */
class AstSrcLocation {
public:
    /** A class locating a single point in an input file */
    struct Point {
        /** The line in the source file */
        int line;

        /** The column in the source file */
        int column;

        /** A comparison for points */
        bool operator<(const Point& other) const {
            return line < other.line || (line == other.line && column < other.column);
        }

        bool operator>(const Point& other) const {
            return other < *this;
        }

        void print(std::ostream& out) const {
            out << line << ":" << column;
        }

        /** Enables locations to be printed */
        friend std::ostream& operator<<(std::ostream& out, const Point& loc) {
            loc.print(out);
            return out;
        }
    };

    /** The file referred to */
    std::string filename;

    /** The start location */
    Point start;

    /** The End location */
    Point end;

    /** A comparison for source locations */
    bool operator<(const AstSrcLocation& other) const {
        if (filename < other.filename) {
            return true;
        }
        if (filename > other.filename) {
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

    /** An extended string describing this location in a end-user friendly way */
    std::string extloc() const;

    void print(std::ostream& out) const {
        out << filename << " [" << start << "-" << end << "]";
    }

    /** Enables ranges to be printed */
    friend std::ostream& operator<<(std::ostream& out, const AstSrcLocation& range) {
        range.print(out);
        return out;
    }
};

}  // end of namespace souffle
