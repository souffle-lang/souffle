/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file gzfstream.h
 * A simple zlib wrapper to provide gzip file streams.
 *
 ***********************************************************************/

#pragma once

#include <iostream>

#include <zlib.h>

namespace souffle {

namespace gzfstream {

namespace internal {

class gzfstreambuf : public std::streambuf {
public:
    gzfstreambuf() {
        setp(buffer, buffer + (bufferSize - 1));
        setg(buffer + reserveSize, buffer + reserveSize, buffer + reserveSize);
    }

    gzfstreambuf(const gzfstreambuf&) = delete;

    gzfstreambuf(gzfstreambuf&& old) = default;

    gzfstreambuf* open(const std::string& filename, std::ios_base::openmode mode) {
        if (is_open()) {
            return nullptr;
        }
        if (!(mode ^ std::ios::in ^ std::ios::out)) {
            return nullptr;
        }

        this->mode = mode;
        std::string gzmode(mode & std::ios::in ? "rb" : "wb");
        fileHandle = gzopen(filename.c_str(), gzmode.c_str());

        if (!fileHandle) {
            return nullptr;
        } else {
            isOpen = true;
        }

        return this;
    }

    gzfstreambuf* close() {
        if (is_open()) {
            sync();
            isOpen = false;
            if (gzclose(fileHandle) == Z_OK) {
                return this;
            }
        }
        return nullptr;
    }

    bool is_open() const {
        return isOpen;
    }

    ~gzfstreambuf() {
        try {
            close();
        } catch (...) {
            // Don't throw exceptions.
        }
    }

    virtual int overflow(int c = EOF) {
        if (!(mode & std::ios::out) || !isOpen) {
            return EOF;
        }

        if (c != EOF) {
            *pptr() = c;
            pbump(1);
        }
        int toWrite = pptr() - pbase();
        if (gzwrite(fileHandle, pbase(), toWrite) != toWrite) {
            return EOF;
        }
        pbump(-toWrite);

        return c;
    }

    virtual int underflow() {
        if (!(mode & std::ios::in) || !isOpen) {
            return EOF;
        }
        if (gptr() && (gptr() < egptr())) {
            return *reinterpret_cast<unsigned char*>(gptr());
        }

        unsigned charsPutBack = gptr() - eback();
        if (charsPutBack > reserveSize) {
            charsPutBack = reserveSize;
        }
        memcpy(buffer + reserveSize - charsPutBack, gptr() - charsPutBack, charsPutBack);

        int charsRead = gzread(fileHandle, buffer + reserveSize, bufferSize - reserveSize);
        if (charsRead <= 0) {
            return EOF;
        }

        setg(buffer + reserveSize - charsPutBack, buffer + reserveSize, buffer + reserveSize + charsRead);

        return *reinterpret_cast<unsigned char*>(gptr());
    }

    virtual int sync() {
        if (pptr() && pptr() > pbase()) {
            int toWrite = pptr() - pbase();
            if (gzwrite(fileHandle, pbase(), toWrite) != toWrite) {
                return -1;
            }
            pbump(-toWrite);
        }
        return 0;
    }

private:
    // TODO(mmcgr): Determine optimal sizes for the buffer and putback reserve space.
    static constexpr unsigned int bufferSize = 4096;
    static constexpr unsigned int reserveSize = 16;

    char buffer[bufferSize];
    gzFile fileHandle;
    bool isOpen = false;
    std::ios_base::openmode mode;
};

class gzfstream : virtual public std::ios {
public:
    gzfstream() {
        init(&buf);
    }

    gzfstream(const std::string& filename, std::ios_base::openmode mode) {
        init(&buf);
        open(filename, mode);
    }

    gzfstream(const gzfstream&) = delete;

    gzfstream(gzfstream&&) = default;

    ~gzfstream() {}

    void open(const std::string& filename, std::ios_base::openmode mode) {
        if (!buf.open(filename, mode)) {
            clear(rdstate() | std::ios::badbit);
        }
    }

    bool is_open() {
        return buf.is_open();
    }

    void close() {
        if (buf.is_open())
            if (!buf.close()) {
                clear(rdstate() | std::ios::badbit);
            }
    }

    gzfstreambuf* rdbuf() const {
        return &buf;
    }

protected:
    mutable gzfstreambuf buf;
};

} /* namespace gzfstream::internal */

class igzfstream : public internal::gzfstream, public std::istream {
public:
    igzfstream() : internal::gzfstream(), std::istream(&buf) {}

    explicit igzfstream(const std::string& filename, std::ios_base::openmode mode = std::ios::in)
            : internal::gzfstream(filename, mode), std::istream(&buf) {}

    igzfstream(const igzfstream&) = delete;

    igzfstream(igzfstream&&) = default;

    internal::gzfstreambuf* rdbuf() const {
        return internal::gzfstream::rdbuf();
    }

    void open(const std::string& filename, std::ios_base::openmode mode = std::ios::in) {
        internal::gzfstream::open(filename, mode);
    }
};

class ogzfstream : public internal::gzfstream, public std::ostream {
public:
    ogzfstream() : std::ostream(&buf) {}

    explicit ogzfstream(const std::string& filename, std::ios_base::openmode mode = std::ios::out)
            : internal::gzfstream(filename, mode), std::ostream(&buf) {}

    ogzfstream(const ogzfstream&) = delete;

    ogzfstream(ogzfstream&&) = default;

    internal::gzfstreambuf* rdbuf() const {
        return internal::gzfstream::rdbuf();
    }

    void open(const std::string& filename, std::ios_base::openmode mode = std::ios::out) {
        internal::gzfstream::open(filename, mode);
    }
};

} /* namespace gzfstream */

} /* namespace souffle */
