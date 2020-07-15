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

#include "souffle/RamTypes.h"
#include "souffle/RecordTable.h"
#include "souffle/SymbolTable.h"
#include "souffle/io/SerialisationStream.h"
#include "souffle/json11.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <ostream>
#include <string>

namespace souffle {

using json11::Json;

class WriteStream : public SerialisationStream<true> {
public:
    WriteStream(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : SerialisationStream(symbolTable, recordTable, rwOperation),
              summary(rwOperation.at("IO") == "stdoutprintsize") {}

    template <typename T>
    void writeAll(const T& relation) {
        if (summary) {
            return writeSize(relation.size());
        }
        auto lease = symbolTable.acquireLock();
        (void)lease;  // silence "unused variable" warning
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

    template <typename T>
    void writeSize(const T& relation) {
        writeSize(relation.size());
    }

protected:
    const bool summary;

    virtual void writeNullary() = 0;
    virtual void writeNextTuple(const RamDomain* tuple) = 0;
    virtual void writeSize(std::size_t) {
        fatal("attempting to print size of a write operation");
    }

    template <typename Tuple>
    void writeNext(const Tuple tuple) {
        writeNextTuple(tuple.data);
    }

    void outputRecord(std::ostream& destination, const RamDomain value, const std::string& name) {
        auto&& recordInfo = types["records"][name];

        // Check if record type information are present
        assert(!recordInfo.is_null() && "Missing record type information");

        // Check for nil
        if (value == 0) {
            destination << "nil";
            return;
        }

        auto&& recordTypes = recordInfo["types"];
        const size_t recordArity = recordInfo["arity"].long_value();

        const RamDomain* tuplePtr = recordTable.unpack(value, recordArity);

        destination << "[";

        // print record's elements
        for (size_t i = 0; i < recordArity; ++i) {
            if (i > 0) {
                destination << ", ";
            }

            const std::string& recordType = recordTypes[i].string_value();
            const RamDomain recordValue = tuplePtr[i];

            switch (recordType[0]) {
                case 'i': destination << recordValue; break;
                case 'f': destination << ramBitCast<RamFloat>(recordValue); break;
                case 'u': destination << ramBitCast<RamUnsigned>(recordValue); break;
                case 's': destination << symbolTable.unsafeResolve(recordValue); break;
                case 'r': outputRecord(destination, recordValue, recordType); break;
                default: fatal("Unsupported type attribute: `%c`", recordType[0]);
            }
        }
        destination << "]";
    }
};

class WriteStreamFactory {
public:
    virtual std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) = 0;

    virtual const std::string& getName() const = 0;
    virtual ~WriteStreamFactory() = default;
};

template <>
inline void WriteStream::writeNext(const RamDomain* tuple) {
    writeNextTuple(tuple);
}

} /* namespace souffle */
