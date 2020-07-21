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

#include "RamTypes.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#include "utility/ContainerUtil.h"
#include "utility/MiscUtil.h"
#include "utility/ParallelUtil.h"
#ifdef USE_LIBZ
#include "gzfstream.h"
#endif

#include <cstddef>
#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace souffle {

class RecordTable;

class WriteStreamCSV : public WriteStream {
protected:
    WriteStreamCSV(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStream(rwOperation, symbolTable, recordTable),
              delimiter(getOr(rwOperation, "delimiter", "\t")){};

    const std::string delimiter;

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
            case 's': destination << symbolTable.unsafeResolve(value); break;
            case 'i': destination << value; break;
            case 'u': destination << ramBitCast<RamUnsigned>(value); break;
            case 'f': destination << ramBitCast<RamFloat>(value); break;
            case 'r': outputRecord(destination, value, type); break;
            default: fatal("unsupported type attribute: `%c`", type[0]);
        }
    }
};

class WriteFileCSV : public WriteStreamCSV {
public:
    WriteFileCSV(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStreamCSV(rwOperation, symbolTable, recordTable),
              file(getFileName(rwOperation), std::ios::out | std::ios::binary) {
        if (getOr(rwOperation, "headers", "false") == "true") {
            file << rwOperation.at("attributeNames") << std::endl;
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

    /**
     * Return given filename or construct from relation name.
     * Default name is [configured path]/[relation name].csv
     *
     * @param rwOperation map of IO configuration options
     * @return input filename
     */
    static std::string getFileName(const std::map<std::string, std::string>& rwOperation) {
        auto name = getOr(rwOperation, "filename", rwOperation.at("name") + ".csv");
        if (name.front() != '/') {
            name = getOr(rwOperation, "output-dir", ".") + "/" + name;
        }
        return name;
    }
};

#ifdef USE_LIBZ
class WriteGZipFileCSV : public WriteStreamCSV {
public:
    WriteGZipFileCSV(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStreamCSV(rwOperation, symbolTable, recordTable),
              file(getFileName(rwOperation), std::ios::out | std::ios::binary) {
        if (getOr(rwOperation, "headers", "false") == "true") {
            file << rwOperation.at("attributeNames") << std::endl;
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

    /**
     * Return given filename or construct from relation name.
     * Default name is [configured path]/[relation name].csv
     *
     * @param rwOperation map of IO configuration options
     * @return input filename
     */
    static std::string getFileName(const std::map<std::string, std::string>& rwOperation) {
        auto name = getOr(rwOperation, "filename", rwOperation.at("name") + ".csv.gz");
        if (name.front() != '/') {
            name = getOr(rwOperation, "output-dir", ".") + "/" + name;
        }
        return name;
    }

    gzfstream::ogzfstream file;
};
#endif

class WriteCoutCSV : public WriteStreamCSV {
public:
    WriteCoutCSV(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStreamCSV(rwOperation, symbolTable, recordTable) {
        std::cout << "---------------\n" << rwOperation.at("name");
        if (getOr(rwOperation, "headers", "false") == "true") {
            std::cout << "\n" << rwOperation.at("attributeNames");
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
    explicit WriteCoutPrintSize(const std::map<std::string, std::string>& rwOperation)
            : WriteStream(rwOperation, {}, {}), lease(souffle::getOutputLock().acquire()) {
        std::cout << rwOperation.at("name") << "\t";
    }

    ~WriteCoutPrintSize() override = default;

protected:
    void writeNullary() override {
        fatal("attempting to iterate over a print size operation");
    }

    void writeNextTuple(const RamDomain* /* tuple */) override {
        fatal("attempting to iterate over a print size operation");
    }

    void writeSize(std::size_t size) override {
        std::cout << size << "\n";
    }

    Lock::Lease lease;
};

class WriteFileCSVFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) override {
#ifdef USE_LIBZ
        if (contains(rwOperation, "compress")) {
            return std::make_unique<WriteGZipFileCSV>(rwOperation, symbolTable, recordTable);
        }
#endif
        return std::make_unique<WriteFileCSV>(rwOperation, symbolTable, recordTable);
    }
    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }
    ~WriteFileCSVFactory() override = default;
};

class WriteCoutCSVFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) override {
        return std::make_unique<WriteCoutCSV>(rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "stdout";
        return name;
    }
    ~WriteCoutCSVFactory() override = default;
};

class WriteCoutPrintSizeFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable&, const RecordTable&) override {
        return std::make_unique<WriteCoutPrintSize>(rwOperation);
    }
    const std::string& getName() const override {
        static const std::string name = "stdoutprintsize";
        return name;
    }
    ~WriteCoutPrintSizeFactory() override = default;
};

} /* namespace souffle */
