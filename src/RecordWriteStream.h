/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecordWriteStream.h
 *
 ***********************************************************************/

#pragma once

#include "IODirectives.h"
#include "RamTypes.h"
#include "SymbolMask.h"
#include "SymbolTable.h"

#include <vector>

namespace souffle {

class RecordWriteStream {
public:
    RecordWriteStream(const SymbolTable& symbolTable) : symbolTable(symbolTable) {}

    void writeAll(const std::vector<std::vector<RamDomain>> records) {
        auto lease = symbolTable.acquireLock();
        (void)lease;
        for (size_t i = 0; i < records.size(); ++i) {
            writeNext(i, records[i]);
        }
    }

    virtual void writeSymbolTable() = 0;

    virtual void writeNextLine(std::string& line) = 0;

    virtual const std::string& getDelimiter() const = 0;

    virtual ~RecordWriteStream() = default;

protected:
    virtual void writeNext(int ind, const std::vector<RamDomain>& record) = 0;
    const SymbolTable& symbolTable;
};

class RecordWriteStreamFactory {
public:
    virtual std::unique_ptr<RecordWriteStream> getWriter(const SymbolTable& symbolTable, const IODirectives& ioDirectives) = 0;

    virtual const std::string& getName() const = 0;
    virtual ~RecordWriteStreamFactory() = default;
};
} /* namespace souffle */
