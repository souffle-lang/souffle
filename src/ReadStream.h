/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReadStream.h
 *
 ***********************************************************************/

#pragma once

#include "IODirectives.h"
#include "RamTypes.h"
#include "SymbolMask.h"
#include "SymbolTable.h"

#include <memory>

namespace souffle {

class ReadStream {
public:
    ReadStream(const SymbolMask& symbolMask, SymbolTable& symbolTable, const bool prov)
            : symbolMask(symbolMask), symbolTable(symbolTable), isProvenance(prov),
              arity(symbolMask.getArity() - (prov ? 2 : 0)) {}
    template <typename T>
    void readAll(T& relation) {
        auto lease = symbolTable.acquireLock();
        (void)lease;
        while (const auto next = readNextTuple()) {
            const RamDomain* ramDomain = next.get();
            relation.insert(ramDomain);
        }
    }

    virtual ~ReadStream() = default;

protected:
    virtual std::unique_ptr<RamDomain[]> readNextTuple() = 0;
    const SymbolMask& symbolMask;
    SymbolTable& symbolTable;
    const bool isProvenance;
    const uint8_t arity;
};

class ReadStreamFactory {
public:
    virtual std::unique_ptr<ReadStream> getReader(const SymbolMask& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) = 0;
    virtual const std::string& getName() const = 0;
    virtual ~ReadStreamFactory() = default;
};

} /* namespace souffle */
