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

class WriteStreamCSV : public WriteStream {
public:
    WriteStreamCSV(std::ostream& out, const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            char delimiter = '\t')
            : delimiter(delimiter), out(out), symbolMask(symbolMask), symbolTable(symbolTable) {}
    void writeNextTuple(const RamDomain* tuple) override {
        if (symbolMask.getArity() == 0) {
            out << "()\n";
            return;
        }

        if (symbolMask.isSymbol(0)) {
            out << symbolTable.resolve(tuple[0]);
        } else {
            out << static_cast<int32_t>(tuple[0]);
        }
        for (size_t col = 1; col < symbolMask.getArity(); ++col) {
            out << delimiter;
            if (symbolMask.isSymbol(col)) {
                std::string s = symbolTable.resolve(tuple[col]);
                out << s;
            } else {
                out << static_cast<int32_t>(tuple[col]);
            }
        }
        out << "\n";
    }

    // optimizing, unsafe version. Doesn't lock, doesn't bound-check.
    void writeNextTupleUnsafe(const RamDomain* tuple) {
        size_t arity = symbolMask.getArity();
        if (arity == 0) {
            out << "()\n";
            return;
        }

        if (symbolMask.isSymbol(0)) {
            out << symbolTable.unsafeResolve(tuple[0]);
        } else {
            out << static_cast<int32_t>(tuple[0]);
        }
        for (size_t col = 1; col < arity; ++col) {
            out << delimiter;
            if (symbolMask.isSymbol(col)) {
                out << symbolTable.unsafeResolve(tuple[col]);
            } else {
                out << static_cast<int32_t>(tuple[col]);
            }
        }
        out << "\n";
    }

    ~WriteStreamCSV() override = default;

private:
    const char delimiter;
    std::ostream& out;
    const SymbolMask& symbolMask;
    const SymbolTable& symbolTable;
};

class WriteFileCSV : public WriteStream {
public:
    WriteFileCSV(const std::string& filename, const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            char delimiter = '\t')
            : file(filename), writeStream(file, symbolMask, symbolTable, delimiter) {}
    void writeNextTuple(const RamDomain* tuple) override {
        writeStream.writeNextTupleUnsafe(tuple);
    }

    ~WriteFileCSV() override = default;

private:
    std::ofstream file;
    WriteStreamCSV writeStream;
};

#ifdef USE_LIBZ
class WriteGZipFileCSV : public WriteStream {
public:
    WriteGZipFileCSV(const std::string& filename, const SymbolMask& symbolMask,
            const SymbolTable& symbolTable, char delimiter = '\t')
            : file(filename), writeStream(file, symbolMask, symbolTable, delimiter) {}
    void writeNextTuple(const RamDomain* tuple) override {
        writeStream.writeNextTuple(tuple);
    }

    ~WriteGZipFileCSV() override = default;

private:
    gzfstream::ogzfstream file;
    WriteStreamCSV writeStream;
};
#endif

class WriteCoutCSV : public WriteStream {
public:
    WriteCoutCSV(const std::string& relationName, const SymbolMask& symbolMask,
            const SymbolTable& symbolTable, char delimiter = '\t')
            : writeStream(std::cout, symbolMask, symbolTable, delimiter) {
        std::cout << "---------------\n" << relationName << "\n===============\n";
    }
    void writeNextTuple(const RamDomain* tuple) override {
        writeStream.writeNextTuple(tuple);
    }

    ~WriteCoutCSV() override {
        std::cout << "===============\n";
    }

private:
    WriteStreamCSV writeStream;
};

class WriteCSVFactory {
protected:
    char getDelimiter(const IODirectives& ioDirectives) {
        char delimiter = '\t';
        if (ioDirectives.has("delimiter")) {
            delimiter = ioDirectives.get("delimiter").at(0);
        }
        return delimiter;
    }
};

class WriteFileCSVFactory : public WriteStreamFactory, public WriteCSVFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives) override {
        char delimiter = getDelimiter(ioDirectives);
#ifdef USE_LIBZ
        if (ioDirectives.has("compress")) {
            return std::unique_ptr<WriteGZipFileCSV>(
                    new WriteGZipFileCSV(ioDirectives.get("filename"), symbolMask, symbolTable, delimiter));
        }
#endif
        return std::unique_ptr<WriteFileCSV>(
                new WriteFileCSV(ioDirectives.get("filename"), symbolMask, symbolTable, delimiter));
    }
    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }
    ~WriteFileCSVFactory() override = default;
};

class WriteCoutCSVFactory : public WriteStreamFactory, public WriteCSVFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives) override {
        char delimiter = getDelimiter(ioDirectives);
        return std::unique_ptr<WriteCoutCSV>(
                new WriteCoutCSV(ioDirectives.getRelationName(), symbolMask, symbolTable, delimiter));
    }
    const std::string& getName() const override {
        static const std::string name = "stdout";
        return name;
    }
    ~WriteCoutCSVFactory() override = default;
};

} /* namespace souffle */
