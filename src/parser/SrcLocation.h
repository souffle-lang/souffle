/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SrcLocation.h
 *
 * Structures to describe a location within input code.
 *
 ***********************************************************************/

#pragma once

#include "VirtualFileSystem.h"

#include <filesystem>
#include <list>
#include <memory>
#include <ostream>
#include <sstream>
#include <stack>
#include <string>

using YY_BUFFER_STATE = struct yy_buffer_state*;

namespace souffle {

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

/** A recursive include stack. */
struct IncludeStack {
    explicit IncludeStack(std::shared_ptr<IncludeStack> parent, Point includePos,
            const std::filesystem::path& physical, const std::string& reported,
            bool reducedConsecutiveNonLeadingWhitespaces = false)
            : ParentStack(parent), IncludePos(includePos), Physical(physical), Reported(reported),
              ReducedConsecutiveNonLeadingWhitespaces(reducedConsecutiveNonLeadingWhitespaces) {}

    /** The parent file. */
    const std::shared_ptr<IncludeStack> ParentStack;

    /** The position of the include directive in the parent file. */
    const Point IncludePos;

    /** This file. */
    const std::filesystem::path Physical;

    /** The reported path for this file in UTF-8 encoding. */
    const std::string Reported;

    /** Indicate if this input had consecutive non-leading whitespace
     * characters replaced by a single space. */
    const bool ReducedConsecutiveNonLeadingWhitespaces;
};

/** A class describing a range in an input file */
class SrcLocation {
public:
    /** Include stack of scanned files, top is the current scanned file. It's
     * not necessarily reflecting the actual input buffers stack. */
    std::shared_ptr<IncludeStack> file;

    /** The start location */
    Point start = {1, 1};

    /** The End location */
    Point end = {1, 1};

    /** Return the shortened reported file name */
    std::string getReportedFilename() const;

    /** Return the full reported file path */
    const std::string& getReportedPath() const;

    /** A comparison for source locations */
    bool operator<(const SrcLocation& other) const;

    /** Extend the current source location with the other, only if both have
     * the same include stack */
    SrcLocation& operator+=(const SrcLocation& other);

    /** Set the source location's file (hence include stack) */
    void setFile(const std::shared_ptr<IncludeStack>& file);

    /** An extended string describing this location in a end-user friendly way */
    std::string extloc() const;

    void print(std::ostream& out) const;

    /** Enables ranges to be printed */
    friend std::ostream& operator<<(std::ostream& out, const SrcLocation& range) {
        range.print(out);
        return out;
    }
};

/** Information struct for scanner */
struct ScannerInfo {
    ScannerInfo(std::shared_ptr<FileSystem> fs) : FS(fs) {}

    /** Scanner's current location */
    SrcLocation yylloc;

    /** Stack of source location cursors for each of the currently opened
     * physical input sources. */
    std::stack<SrcLocation> Frames;

    /** Hold the input buffers for the lifetime of the scanner. */
    std::list<std::shared_ptr<std::string>> InputBuffers;

    /** File system abstraction for this parser. */
    std::shared_ptr<FileSystem> FS;

    /** Location of last .include directive */
    SrcLocation LastIncludeDirectiveLoc;

    /** Extent of the current comment */
    SrcLocation CommentExtent;

    /** Content of the current comment */
    std::stringstream CommentContent;

    /** Push a file on the include stack */
    void push(const std::filesystem::path& PhysicalPath, const SrcLocation& IncludeLoc,
            bool reducedWhitespaces = false);

    /** Pop a file from the include stack */
    void pop();

    /** Set the reported path for the top of the include stack (current file) */
    void setReported(const std::string& Reported);

    /** Hold the given input buffer for the lifetime of the scanner. */
    void holdInputBuffer(std::unique_ptr<std::string> Buffer);
};

}  // end of namespace souffle
