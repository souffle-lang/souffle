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

#include <memory>
#include <string>

namespace souffle {

class WriteStreamCSV : public WriteStream {
public:
    WriteStreamCSV(std::ostream& out, const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            char delimiter = '\t')
            : delimiter(delimiter), out(out), symbolMask(symbolMask), symbolTable(symbolTable) {}
    virtual void writeNextTuple(const RamDomain* tuple) {
        if (symbolMask.getArity() == 0) {
            out << "()\n";
            return;
        }

        if (symbolMask.isSymbol(0)) {
            out << symbolTable.resolve(tuple[0]);
        } else {
            out << (int32_t)tuple[0];
        }
        for (size_t col = 1; col < symbolMask.getArity(); ++col) {
            out << delimiter;
            if (symbolMask.isSymbol(col)) {
                std::string s = symbolTable.resolve(tuple[col]);
                out << s;
            } else {
                out << (int32_t)tuple[col];
            }
        }
        out << "\n";
    }

    virtual ~WriteStreamCSV() {}

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
    virtual void writeNextTuple(const RamDomain* tuple) {
        writeStream.writeNextTuple(tuple);
    }

    virtual ~WriteFileCSV() {}

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
    virtual void writeNextTuple(const RamDomain* tuple) {
        writeStream.writeNextTuple(tuple);
    }

    virtual ~WriteGZipFileCSV() {}

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
    virtual void writeNextTuple(const RamDomain* tuple) {
        writeStream.writeNextTuple(tuple);
    }

    virtual ~WriteCoutCSV() {
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
    virtual std::unique_ptr<WriteStream> getWriter(
            const SymbolMask& symbolMask, const SymbolTable& symbolTable, const IODirectives& ioDirectives) {
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
    virtual const std::string& getName() const {
        return name;
    }
    virtual ~WriteFileCSVFactory() {}

private:
    static const std::string name;
};

const std::string WriteFileCSVFactory::name = "file";

class WriteCoutCSVFactory : public WriteStreamFactory, public WriteCSVFactory {
public:
    virtual std::unique_ptr<WriteStream> getWriter(
            const SymbolMask& symbolMask, const SymbolTable& symbolTable, const IODirectives& ioDirectives) {
        char delimiter = getDelimiter(ioDirectives);
        return std::unique_ptr<WriteCoutCSV>(
                new WriteCoutCSV(ioDirectives.getRelationName(), symbolMask, symbolTable, delimiter));
    }
    virtual const std::string& getName() const {
        return name;
    }
    virtual ~WriteCoutCSVFactory() {}

private:
    static const std::string name;
};

const std::string WriteCoutCSVFactory::name = "stdout";

} /* namespace souffle */
