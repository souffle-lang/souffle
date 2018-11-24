/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file WriteStreamCSV.h
 *
 ***********************************************************************/

#pragma once

#include "IODirectives.h"
#include "SymbolMask.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#ifdef USE_LIBZ
#include "gzfstream.h"
#endif

#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace souffle {

class WriteStreamCSV {
protected:
    virtual std::string getDelimiter(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("delimiter")) {
            return ioDirectives.get("delimiter");
        }
        return "\t";
    }
};

class WriteFileCSV : public WriteStreamCSV, public WriteStream {
public:
    WriteFileCSV(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance = false)
            : WriteStream(symbolMask, symbolTable, provenance), delimiter(getDelimiter(ioDirectives)),
              file(ioDirectives.getFileName(), std::ios::out | std::ios::binary) {
        if (ioDirectives.has("headers") && ioDirectives.get("headers") == "true") {
            file << ioDirectives.get("attributeNames") << std::endl;
        }
    }

    ~WriteFileCSV() override = default;

protected:
    void writeNullary() override {
        file << "()\n";
    }

    void writeNextTuple(const RamDomain* tuple) override {
        if (symbolMask.isSymbol(0)) {
            file << symbolTable.unsafeResolve(tuple[0]);
        } else {
            file << tuple[0];
        }
        for (size_t col = 1; col < arity; ++col) {
            file << delimiter;
            if (symbolMask.isSymbol(col)) {
                file << symbolTable.unsafeResolve(tuple[col]);
            } else {
                file << tuple[col];
            }
        }
        file << "\n";
    }

protected:
    const std::string delimiter;
    std::ofstream file;
};

#ifdef USE_LIBZ
class WriteGZipFileCSV : public WriteStreamCSV, public WriteStream {
public:
    WriteGZipFileCSV(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance = false)
            : WriteStream(symbolMask, symbolTable, provenance), delimiter(getDelimiter(ioDirectives)),
              file(ioDirectives.getFileName(), std::ios::out | std::ios::binary) {
        if (ioDirectives.has("headers") && ioDirectives.get("headers") == "true") {
            file << ioDirectives.get("attributeNames") << std::endl;
        }
    }

    ~WriteGZipFileCSV() override = default;

protected:
    void writeNullary() override {
        file << "()\n";
    }

    void writeNextTuple(const RamDomain* tuple) override {
        if (symbolMask.isSymbol(0)) {
            file << symbolTable.unsafeResolve(tuple[0]);
        } else {
            file << tuple[0];
        }
        for (size_t col = 1; col < arity; ++col) {
            file << delimiter;
            if (symbolMask.isSymbol(col)) {
                file << symbolTable.unsafeResolve(tuple[col]);
            } else {
                file << tuple[col];
            }
        }
        file << "\n";
    }

    const std::string delimiter;
    gzfstream::ogzfstream file;
};
#endif

class WriteCoutCSV : public WriteStreamCSV, public WriteStream {
public:
    WriteCoutCSV(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance = false)
            : WriteStream(symbolMask, symbolTable, provenance), delimiter(getDelimiter(ioDirectives)) {
        std::cout << "---------------\n" << ioDirectives.getRelationName();
        if (ioDirectives.has("headers") && ioDirectives.get("headers") == "true") {
            std::cout << "\n" << ioDirectives.get("attributeNames");
        }
        std::cout << "\n===============\n";
    }

    ~WriteCoutCSV() override {
        std::cout << "===============\n";
    }

protected:
    void writeNullary() override {
        std::cout << "()\n";
    }

    void writeNextTuple(const RamDomain* tuple) override {
        if (symbolMask.isSymbol(0)) {
            std::cout << symbolTable.unsafeResolve(tuple[0]);
        } else {
            std::cout << tuple[0];
        }
        for (size_t col = 1; col < arity; ++col) {
            std::cout << delimiter;
            if (symbolMask.isSymbol(col)) {
                std::cout << symbolTable.unsafeResolve(tuple[col]);
            } else {
                std::cout << tuple[col];
            }
        }
        std::cout << "\n";
    }

    const std::string delimiter;
};

class WriteFileCSVFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) override {
#ifdef USE_LIBZ
        if (ioDirectives.has("compress")) {
            return std::make_unique<WriteGZipFileCSV>(symbolMask, symbolTable, ioDirectives, provenance);
        }
#endif
        return std::make_unique<WriteFileCSV>(symbolMask, symbolTable, ioDirectives, provenance);
    }
    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }
    ~WriteFileCSVFactory() override = default;
};

class WriteCoutCSVFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) override {
        return std::make_unique<WriteCoutCSV>(symbolMask, symbolTable, ioDirectives, provenance);
    }
    const std::string& getName() const override {
        static const std::string name = "stdout";
        return name;
    }
    ~WriteCoutCSVFactory() override = default;
};

} /* namespace souffle */
