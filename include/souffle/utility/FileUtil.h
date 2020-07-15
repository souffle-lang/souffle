/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file FileUtil.h
 *
 * @brief Datalog project utilities
 *
 ***********************************************************************/

#pragma once

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <windows.h>

// -------------------------------------------------------------------------------
//                               File Utils
// -------------------------------------------------------------------------------

#define X_OK 1 /* execute permission - unsupported in windows*/

#define PATH_MAX 260

/**
 * access and realpath are missing on windows, we use their windows equivalents
 * as work-arounds.
 */
#define access _access
inline char* realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, PATH_MAX);
}

#endif

namespace souffle {

/**
 *  Check whether a file exists in the file system
 */
inline bool existFile(const std::string& name) {
    struct stat buffer = {};
    if (stat(name.c_str(), &buffer) == 0) {
        if ((buffer.st_mode & S_IFMT) != 0) {
            return true;
        }
    }
    return false;
}

/**
 *  Check whether a directory exists in the file system
 */
inline bool existDir(const std::string& name) {
    struct stat buffer = {};
    if (stat(name.c_str(), &buffer) == 0) {
        if ((buffer.st_mode & S_IFDIR) != 0) {
            return true;
        }
    }
    return false;
}

/**
 * Check whether a given file exists and it is an executable
 */
inline bool isExecutable(const std::string& name) {
    return existFile(name) && (access(name.c_str(), X_OK) == 0);
}

/**
 * Simple implementation of a which tool
 */
inline std::string which(const std::string& name) {
    // Check if name has path components in it and if so return it immediately
    if (name.find('/') != std::string::npos) {
        return name;
    }
    // Get PATH from environment, if it exists.
    const char* syspath = ::getenv("PATH");
    if (syspath == nullptr) {
        return "";
    }
    char buf[PATH_MAX];
    std::stringstream sstr;
    sstr << syspath;
    std::string sub;

    // Check for existence of a binary called 'name' in PATH
    while (std::getline(sstr, sub, ':')) {
        std::string path = sub + "/" + name;
        if ((::realpath(path.c_str(), buf) != nullptr) && isExecutable(path) && !existDir(path)) {
            return buf;
        }
    }
    return "";
}

/**
 *  C++-style dirname
 */
inline std::string dirName(const std::string& name) {
    if (name.empty()) {
        return ".";
    }
    size_t lastNotSlash = name.find_last_not_of('/');
    // All '/'
    if (lastNotSlash == std::string::npos) {
        return "/";
    }
    size_t leadingSlash = name.find_last_of('/', lastNotSlash);
    // No '/'
    if (leadingSlash == std::string::npos) {
        return ".";
    }
    // dirname is '/'
    if (leadingSlash == 0) {
        return "/";
    }
    return name.substr(0, leadingSlash);
}

/**
 *  C++-style realpath
 */
inline std::string absPath(const std::string& path) {
    char buf[PATH_MAX];
    char* res = realpath(path.c_str(), buf);
    return (res == nullptr) ? "" : std::string(buf);
}

/**
 *  Join two paths together; note that this does not resolve overlaps or relative paths.
 */
inline std::string pathJoin(const std::string& first, const std::string& second) {
    unsigned firstPos = static_cast<unsigned>(first.size()) - 1;
    while (first.at(firstPos) == '/') {
        firstPos--;
    }
    unsigned secondPos = 0;
    while (second.at(secondPos) == '/') {
        secondPos++;
    }
    return first.substr(0, firstPos + 1) + '/' + second.substr(secondPos);
}

/*
 * Find out if an executable given by @p tool exists in the path given @p path
 * relative to the directory given by @ base. A path here refers a
 * colon-separated list of directories.
 */
inline std::string findTool(const std::string& tool, const std::string& base, const std::string& path) {
    std::string dir = dirName(base);
    std::stringstream sstr(path);
    std::string sub;

    while (std::getline(sstr, sub, ':')) {
        std::string subpath = dir + "/" + sub + '/' + tool;
        if (isExecutable(subpath)) {
            return absPath(subpath);
        }
    }
    return "";
}

/*
 * Get the basename of a fully qualified filename
 */
inline std::string baseName(const std::string& filename) {
    if (filename.empty()) {
        return ".";
    }

    size_t lastNotSlash = filename.find_last_not_of('/');
    if (lastNotSlash == std::string::npos) {
        return "/";
    }

    size_t lastSlashBeforeBasename = filename.find_last_of('/', lastNotSlash - 1);
    if (lastSlashBeforeBasename == std::string::npos) {
        lastSlashBeforeBasename = static_cast<size_t>(-1);
    }
    return filename.substr(lastSlashBeforeBasename + 1, lastNotSlash - lastSlashBeforeBasename);
}

/**
 * File name, with extension removed.
 */
inline std::string simpleName(const std::string& path) {
    std::string name = baseName(path);
    const size_t lastDot = name.find_last_of('.');
    // file has no extension
    if (lastDot == std::string::npos) {
        return name;
    }
    const size_t lastSlash = name.find_last_of('/');
    // last slash occurs after last dot, so no extension
    if (lastSlash != std::string::npos && lastSlash > lastDot) {
        return name;
    }
    // last dot after last slash, or no slash
    return name.substr(0, lastDot);
}

/**
 * File extension, with all else removed.
 */
inline std::string fileExtension(const std::string& path) {
    std::string name = path;
    const size_t lastDot = name.find_last_of('.');
    // file has no extension
    if (lastDot == std::string::npos) {
        return std::string();
    }
    const size_t lastSlash = name.find_last_of('/');
    // last slash occurs after last dot, so no extension
    if (lastSlash != std::string::npos && lastSlash > lastDot) {
        return std::string();
    }
    // last dot after last slash, or no slash
    return name.substr(lastDot + 1);
}

/**
 * Generate temporary file.
 */
inline std::string tempFile() {
#ifdef _WIN32
    std::string templ;
    std::FILE* f = nullptr;
    while (f == nullptr) {
        templ = std::tmpnam(nullptr);
        f = fopen(templ.c_str(), "wx");
    }
    fclose(f);
    return templ;
#else
    char templ[40] = "./souffleXXXXXX";
    close(mkstemp(templ));
    return std::string(templ);
#endif
}

inline std::stringstream execStdOut(char const* cmd) {
    FILE* in = popen(cmd, "r");
    std::stringstream data;
    while (in != nullptr) {
        char c = fgetc(in);
        if (feof(in) != 0) {
            break;
        }
        data << c;
    }
    pclose(in);
    return data;
}

inline std::stringstream execStdOut(std::string const& cmd) {
    return execStdOut(cmd.c_str());
}

class TempFileStream : public std::fstream {
    std::string fileName;

public:
    TempFileStream(std::string fileName = tempFile())
            : std::fstream(fileName), fileName(std::move(fileName)) {}
    ~TempFileStream() override {
        close();
        remove(fileName.c_str());
    }

    std::string const& getFileName() const {
        return fileName;
    }
};

}  // namespace souffle
