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
#include "SymbolMask.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#include "WriteStreamCSV.h"

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

    void registerReadStreamFactory(std::shared_ptr<ReadStreamFactory> factory) {
        inputFactories[factory->getName()] = factory;
    }

    /**
     * Return a new WriteStream
     */
    std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
            const IODirectives& ioDirectives) const {
        std::string ioType = ioDirectives.getIOType();
        if (outputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested output type <" + ioType + "> is not supported.");
        }
        return outputFactories.at(ioType)->getWriter(symbolMask, symbolTable, ioDirectives);
    }
    /**
     * Return a new ReadStream
     */
    std::unique_ptr<ReadStream> getReader(
            const SymbolMask& symbolMask, SymbolTable& symbolTable, const IODirectives& ioDirectives) const {
        std::string ioType = ioDirectives.getIOType();
        if (inputFactories.count(ioType) == 0) {
            throw std::invalid_argument("Requested input type <" + ioType + "> is not supported.");
        }
        return inputFactories.at(ioType)->getReader(symbolMask, symbolTable, ioDirectives);
    }
    ~IOSystem() = default;

private:
    IOSystem() {
        registerReadStreamFactory(std::make_shared<ReadFileCSVFactory>());
        registerReadStreamFactory(std::make_shared<ReadCinCSVFactory>());
        registerWriteStreamFactory(std::make_shared<WriteFileCSVFactory>());
        registerWriteStreamFactory(std::make_shared<WriteCoutCSVFactory>());
#ifdef USE_SQLITE
        registerReadStreamFactory(std::make_shared<ReadStreamSQLiteFactory>());
        registerWriteStreamFactory(std::make_shared<WriteSQLiteFactory>());
#endif
    };
    std::map<std::string, std::shared_ptr<WriteStreamFactory>> outputFactories;
    std::map<std::string, std::shared_ptr<ReadStreamFactory>> inputFactories;
};

} /* namespace souffle */
