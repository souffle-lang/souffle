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

std::string SrcLocation::getReportedFilename() const {
    static const std::string emptyFilename("");
    if (file) {
        return std::filesystem::path(file->Reported).filename().string();
    } else {
        return emptyFilename;
    }
}

const std::string& SrcLocation::getReportedPath() const {
    static const std::string emptyFilename("");
    if (file) {
        return file->Reported;
    } else {
        return emptyFilename;
    }
}

bool SrcLocation::operator<(const SrcLocation& other) const {
    const std::string& filename = getReportedPath();
    const std::string& otherFilename = other.getReportedPath();

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

SrcLocation& SrcLocation::operator+=(const SrcLocation& other) {
    if (file.get() == other.file.get()) {
        if (*this < other) {
            end = other.end;
        } else {
            start = other.start;
        }
    }
    return *this;
}

void SrcLocation::setFile(const std::shared_ptr<IncludeStack>& f) {
    file = f;
}

std::string SrcLocation::extloc() const {
    std::ifstream in(file->Reported);
    std::stringstream s;
    if (in.is_open()) {
        s << "file " << getReportedFilename() << " at line " << start.line << "\n";
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
                if (file->ReducedConsecutiveNonLeadingWhitespaces &&
                        (afterFirstNonSpace && prevWhitespace && offsetColumn >= lineLen)) {
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
        s << getReportedFilename() << ":" << start.line << ":" << start.column;
    }
    return s.str();
}

void SrcLocation::print(std::ostream& out) const {
    out << getReportedFilename() << " [" << start << "-" << end << "]";
}

void ScannerInfo::push(
        const std::filesystem::path& Physical, const SrcLocation& IncludeLoc, bool reducedWhitespaces) {
    auto NewFile = std::make_shared<IncludeStack>(
            yylloc.file, IncludeLoc.start, Physical, Physical.u8string(), reducedWhitespaces);
    Frames.push(yylloc);
    yylloc.file = NewFile;
    yylloc.start = yylloc.end = {1, 1};
}

void ScannerInfo::pop() {
    if (!Frames.empty()) {
        yylloc = Frames.top();
        Frames.pop();
    }
}

void ScannerInfo::setReported(const std::string& Reported) {
    if (yylloc.file && yylloc.file->Reported != Reported) {
        yylloc.file = std::make_shared<IncludeStack>(yylloc.file->ParentStack, yylloc.file->IncludePos,
                yylloc.file->Physical, Reported, yylloc.file->ReducedConsecutiveNonLeadingWhitespaces);
    }
}

void ScannerInfo::holdInputBuffer(std::unique_ptr<std::string> Buffer) {
    InputBuffers.push_back(std::move(Buffer));
}

}  // end of namespace souffle
