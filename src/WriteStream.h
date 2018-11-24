/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file WriteStream.h
 *
 ***********************************************************************/

#pragma once

#include "IODirectives.h"
#include "RamTypes.h"
#include "SymbolMask.h"
#include "SymbolTable.h"

namespace souffle {

class WriteStream {
public:
    WriteStream(const SymbolMask& symbolMask, const SymbolTable& symbolTable, const bool prov)
            : symbolMask(symbolMask), symbolTable(symbolTable), isProvenance(prov),
              arity(symbolMask.getArity() - (prov ? 2 : 0)) {}
    template <typename T>
    void writeAll(const T& relation) {
        auto lease = symbolTable.acquireLock();
        (void)lease;
        if (arity == 0) {
            if (relation.begin() != relation.end()) {
                writeNullary();
            }
            return;
        }
        for (const auto& current : relation) {
            writeNext(current);
        }
    }

    virtual ~WriteStream() = default;

protected:
    virtual void writeNullary() = 0;
    virtual void writeNextTuple(const RamDomain* tuple) = 0;
    template <typename Tuple>
    void writeNext(const Tuple tuple) {
        writeNextTuple(tuple.data);
    }
    const SymbolMask& symbolMask;
    const SymbolTable& symbolTable;
    const bool isProvenance;
    const uint8_t arity;
};

class WriteStreamFactory {
public:
    virtual std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask,
            const SymbolTable& symbolTable, const IODirectives& ioDirectives, const bool provenance) = 0;
    virtual const std::string& getName() const = 0;
    virtual ~WriteStreamFactory() = default;
};

template <>
inline void WriteStream::writeNext(const RamDomain* tuple) {
    writeNextTuple(tuple);
}

} /* namespace souffle */
