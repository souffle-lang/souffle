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

#include "IODirectives.h"
#include "ReadStream.h"
#include "ReadStreamCSV.h"
#include "RecordReadStream.h"
#include "RecordReadStreamCSV.h"
#include "SymbolMask.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#include "WriteStreamCSV.h"
#include "RecordWriteStream.h"
#include "RecordWriteStreamCSV.h"

#ifdef USE_SQLITE
#include "ReadStreamSQLite.h"
#include "WriteStreamSQLite.h"
#endif

#include <map>
#include <memory>
#include <string>

namespace souffle {

class IOSystem {
public:
    static IOSystem& getInstance() {
        static IOSystem singleton;
        return singleton;
    }

    void registerWriteStreamFactory(std::shared_ptr<WriteStreamFactory> factory) {
        outputFactories[factory->getName()] = factory;
    }

    void registerRecordWriteStreamFactory(std::shared_ptr<RecordWriteStreamFactory> factory) {
        outputRecordFactories[factory->getName()] = factory;
    }

    void registerReadStreamFactory(std::shared_ptr<ReadStreamFactory> factory) {
        inputFactories[factory->getName()] = factory;
    }

    void registerRecordReadStreamFactory(std::shared_ptr<RecordReadStreamFactory> factory) {
        inputRecordFactories[factory->getName()] = factory;
    }

    /**
     * Return a new WriteStream
     */
    std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) const {
        std::string ioType = ioDirectives.getIOType();
        if (outputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested output type <" + ioType + "> is not supported.");
        }
        return outputFactories.at(ioType)->getWriter(symbolMask, symbolTable, ioDirectives, provenance);
    }
    /**
     * Return a new ReadStream
     */
    std::unique_ptr<ReadStream> getReader(const SymbolMask& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) const {
        std::string ioType = ioDirectives.getIOType();
        if (inputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested input type <" + ioType + "> is not supported.");
        }
        return inputFactories.at(ioType)->getReader(symbolMask, symbolTable, ioDirectives, provenance);
    }
    /**
     * Return a new RecordReadStream
     */
    std::unique_ptr<RecordReadStream> getRecordReader(SymbolTable& symbolTable, const IODirectives& ioDirectives) const {
        std::string ioType = ioDirectives.getIOType();
        if (inputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested input type <" + ioType + "> is not supported.");
        }
        return inputRecordFactories.at(ioType)->getReader(symbolTable, ioDirectives);
    }
    /**
     * Return a new RecordWriteStream
     */
    std::unique_ptr<RecordWriteStream> getRecordWriter(const SymbolTable& symbolTable, const IODirectives& ioDirectives) const {
        std::string ioType = ioDirectives.getIOType();
        if (outputRecordFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested output type <" + ioType + "> is not supported.");
        }
        return outputRecordFactories.at(ioType)->getWriter(symbolTable, ioDirectives);
    }

    ~IOSystem() = default;

private:
    IOSystem() {
        registerReadStreamFactory(std::make_shared<ReadFileCSVFactory>());
        registerReadStreamFactory(std::make_shared<ReadCinCSVFactory>());
        registerWriteStreamFactory(std::make_shared<WriteFileCSVFactory>());
        registerWriteStreamFactory(std::make_shared<WriteCoutCSVFactory>());
        registerRecordReadStreamFactory(std::make_shared<RecordReadFileCSVFactory>());
	registerRecordWriteStreamFactory(std::make_shared<RecordWriteCoutCSVFactory>());
	registerRecordWriteStreamFactory(std::make_shared<RecordWriteFileCSVFactory>());
#ifdef USE_SQLITE
        registerReadStreamFactory(std::make_shared<ReadSQLiteFactory>());
        registerWriteStreamFactory(std::make_shared<WriteSQLiteFactory>());
#endif
    };
    std::map<std::string, std::shared_ptr<WriteStreamFactory>> outputFactories;
    std::map<std::string, std::shared_ptr<ReadStreamFactory>> inputFactories;

    std::map<std::string, std::shared_ptr<RecordReadStreamFactory>> inputRecordFactories;
    std::map<std::string, std::shared_ptr<RecordWriteStreamFactory>> outputRecordFactories;
};

} /* namespace souffle */
