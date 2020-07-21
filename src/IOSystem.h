/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file IOSystem.h
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "ReadStream.h"
#include "ReadStreamCSV.h"
#include "ReadStreamJSON.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#include "WriteStreamCSV.h"
#include "WriteStreamJSON.h"

#ifdef USE_SQLITE
#include "ReadStreamSQLite.h"
#include "WriteStreamSQLite.h"
#endif

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace souffle {
class RecordTable;

class IOSystem {
public:
    static IOSystem& getInstance() {
        static IOSystem singleton;
        return singleton;
    }

    void registerWriteStreamFactory(const std::shared_ptr<WriteStreamFactory>& factory) {
        outputFactories[factory->getName()] = factory;
    }

    void registerReadStreamFactory(const std::shared_ptr<ReadStreamFactory>& factory) {
        inputFactories[factory->getName()] = factory;
    }

    /**
     * Return a new WriteStream
     */
    std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) const {
        std::string ioType = rwOperation.at("IO");
        if (outputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested output type <" + ioType + "> is not supported.");
        }
        return outputFactories.at(ioType)->getWriter(rwOperation, symbolTable, recordTable);
    }
    /**
     * Return a new ReadStream
     */
    std::unique_ptr<ReadStream> getReader(const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable) const {
        std::string ioType = rwOperation.at("IO");
        if (inputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested input type <" + ioType + "> is not supported.");
        }
        return inputFactories.at(ioType)->getReader(rwOperation, symbolTable, recordTable);
    }
    ~IOSystem() = default;

private:
    IOSystem() {
        registerReadStreamFactory(std::make_shared<ReadFileCSVFactory>());
        registerReadStreamFactory(std::make_shared<ReadCinCSVFactory>());
        registerReadStreamFactory(std::make_shared<ReadFileJSONFactory>());
        registerReadStreamFactory(std::make_shared<ReadCinJSONFactory>());
        registerWriteStreamFactory(std::make_shared<WriteFileCSVFactory>());
        registerWriteStreamFactory(std::make_shared<WriteCoutCSVFactory>());
        registerWriteStreamFactory(std::make_shared<WriteCoutPrintSizeFactory>());
        registerWriteStreamFactory(std::make_shared<WriteFileJSONFactory>());
        registerWriteStreamFactory(std::make_shared<WriteCoutJSONFactory>());
#ifdef USE_SQLITE
        registerReadStreamFactory(std::make_shared<ReadSQLiteFactory>());
        registerWriteStreamFactory(std::make_shared<WriteSQLiteFactory>());
#endif
    };
    std::map<std::string, std::shared_ptr<WriteStreamFactory>> outputFactories;
    std::map<std::string, std::shared_ptr<ReadStreamFactory>> inputFactories;
};

} /* namespace souffle */
