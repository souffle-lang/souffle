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
#include "ParallelUtils.h"
#include "RamTypes.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#ifdef USE_LIBZ
#include "gzfstream.h"
#endif

#include <cassert>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>

namespace souffle {

class WriteStreamCSV : public WriteStream {
protected:
    WriteStreamCSV(
            const IODirectives& ioDirectives, const SymbolTable& symbolTable, const RecordTable& recordTable)
            : WriteStream(ioDirectives, symbolTable, recordTable), delimiter(getDelimiter(ioDirectives)){};

    const std::string delimiter;

    std::string getDelimiter(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("delimiter")) {
            return ioDirectives.get("delimiter");
        }
        return "\t";
    }

    void writeNextTupleCSV(std::ostream& destination, const RamDomain* tuple) {
        writeNextTupleElement(destination, typeAttributes.at(0), tuple[0]);

        for (size_t col = 1; col < arity; ++col) {
            destination << delimiter;
            writeNextTupleElement(destination, typeAttributes.at(col), tuple[col]);
        }

        destination << "\n";
    }

    void writeNextTupleElement(std::ostream& destination, const std::string& type, RamDomain value) {
        switch (type[0]) {
            case 's':
                destination << symbolTable.unsafeResolve(value);
                break;
            case 'i':
                destination << value;
                break;
            case 'u':
                destination << ramBitCast<RamUnsigned>(value);
                break;
            case 'f':
                destination << ramBitCast<RamFloat>(value);
                break;
            case 'r':
                outputRecord(destination, value, type);
                break;
            case '+':
                outputSum(destination, value, type);
                break;
            default:
                assert(false && "Unsupported type attribute");
        }
    }
};

class WriteFileCSV : public WriteStreamCSV {
public:
    WriteFileCSV(
            const IODirectives& ioDirectives, const SymbolTable& symbolTable, const RecordTable& recordTable)
            : WriteStreamCSV(ioDirectives, symbolTable, recordTable),
              file(ioDirectives.getFileName(), std::ios::out | std::ios::binary) {
        if (ioDirectives.has("headers") && ioDirectives.get("headers") == "true") {
            file << ioDirectives.get("attributeNames") << std::endl;
        }
    }

    ~WriteFileCSV() override = default;

protected:
    std::ofstream file;

    void writeNullary() override {
        file << "()\n";
    }

    void writeNextTuple(const RamDomain* tuple) override {
        writeNextTupleCSV(file, tuple);
    }
};

#ifdef USE_LIBZ
class WriteGZipFileCSV : public WriteStreamCSV {
public:
    WriteGZipFileCSV(
            const IODirectives& ioDirectives, const SymbolTable& symbolTable, const RecordTable& recordTable)
            : WriteStreamCSV(ioDirectives, symbolTable, recordTable),
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
        writeNextTupleCSV(file, tuple);
    }

    gzfstream::ogzfstream file;
};
#endif

class WriteCoutCSV : public WriteStreamCSV {
public:
    WriteCoutCSV(
            const IODirectives& ioDirectives, const SymbolTable& symbolTable, const RecordTable& recordTable)
            : WriteStreamCSV(ioDirectives, symbolTable, recordTable) {
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
        writeNextTupleCSV(std::cout, tuple);
    }
};

class WriteCoutPrintSize : public WriteStream {
public:
    explicit WriteCoutPrintSize(const IODirectives& ioDirectives)
            : WriteStream(ioDirectives, {}, {}), lease(souffle::getOutputLock().acquire()) {
        std::cout << ioDirectives.getRelationName() << "\t";
    }

    ~WriteCoutPrintSize() override = default;

protected:
    void writeNullary() override {
        assert(false && "attempting to iterate over a print size operation");
    }

    void writeNextTuple(const RamDomain* /* tuple */) override {
        assert(false && "attempting to iterate over a print size operation");
    }

    void writeSize(std::size_t size) override {
        std::cout << size << "\n";
    }

    Lock::Lease lease;
};

class WriteFileCSVFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const IODirectives& ioDirectives, const SymbolTable& symbolTable,
            const RecordTable& recordTable) override {
#ifdef USE_LIBZ
        if (ioDirectives.has("compress")) {
            return std::make_unique<WriteGZipFileCSV>(ioDirectives, symbolTable, recordTable);
        }
#endif
        return std::make_unique<WriteFileCSV>(ioDirectives, symbolTable, recordTable);
    }
    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }
    ~WriteFileCSVFactory() override = default;
};

class WriteCoutCSVFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const IODirectives& ioDirectives, const SymbolTable& symbolTable,
            const RecordTable& recordTable) override {
        return std::make_unique<WriteCoutCSV>(ioDirectives, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "stdout";
        return name;
    }
    ~WriteCoutCSVFactory() override = default;
};

class WriteCoutPrintSizeFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(
            const IODirectives& ioDirectives, const SymbolTable&, const RecordTable&) override {
        return std::make_unique<WriteCoutPrintSize>(ioDirectives);
    }
    const std::string& getName() const override {
        static const std::string name = "stdoutprintsize";
        return name;
    }
    ~WriteCoutPrintSizeFactory() override = default;
};

} /* namespace souffle */
