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
#include "RecordTable.h"
#include "SymbolTable.h"
#include "json11.h"

#include <cassert>
#include <string>
#include <vector>

namespace souffle {

using json11::Json;

class WriteStream {
public:
    WriteStream(
            const IODirectives& ioDirectives, const SymbolTable& symbolTable, const RecordTable& recordTable)
            : symbolTable(symbolTable), recordTable(recordTable),
              summary(ioDirectives.getIOType() == "stdoutprintsize") {
        const std::string& relationName{ioDirectives.getRelationName()};

        std::string parseErrors;

        types = Json::parse(ioDirectives.get("types"), parseErrors);

        assert(parseErrors.size() == 0 && "Internal JSON parsing failed.");

        arity = static_cast<size_t>(types[relationName]["arity"].long_value());

        for (size_t i = 0; i < arity; ++i) {
            std::string type = types[relationName]["types"][i].string_value();
            typeAttributes.push_back(std::move(type));
        }
    }

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

    virtual ~WriteStream() = default;

protected:
    const SymbolTable& symbolTable;
    const RecordTable& recordTable;

    Json types;
    std::vector<std::string> typeAttributes;

    const bool summary;
    size_t arity;

    virtual void writeNullary() = 0;
    virtual void writeNextTuple(const RamDomain* tuple) = 0;
    virtual void writeSize(std::size_t) {
        assert(false && "attempting to print size of a write operation");
    }
    template <typename Tuple>
    void writeNext(const Tuple tuple) {
        writeNextTuple(tuple.data);
    }

    void outputRecord(std::ostream& destination, const RamDomain value, const std::string& name) {
        Json recordInfo = types["records"][name];

        // Check if record type information are present
        if (recordInfo.is_null()) {
            std::cerr << "Missing record type information: " << name << std::endl;
            abort();
        }

        // Check for nil
        if (recordTable.isNil(value)) {
            destination << "nil";
            return;
        }

        const Json recordTypes = recordInfo["types"];
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
                case 'i':
                    destination << recordValue;
                    break;
                case 'f':
                    destination << ramBitCast<RamFloat>(recordValue);
                    break;
                case 'u':
                    destination << ramBitCast<RamUnsigned>(recordValue);
                    break;
                case 's':
                    destination << symbolTable.unsafeResolve(recordValue);
                    break;
                case 'r':
                    outputRecord(destination, recordValue, recordType);
                    break;
                case '+':
                    outputSum(destination, recordValue, recordType);
                    break;
                default:
                    assert(false && "Unsupported type attribute.");
            }
        }
        destination << "]";
    }

    void outputSum(std::ostream& destination, const RamDomain value, const std::string& name) {
        auto const sumInfo = types["sums"][name];
        if (!sumInfo.is_array()) {
            std::cerr << "Missing sum type information: " << name << std::endl;
            abort();
        }

        // sum types are encoded as `(branch-idx, branch-value)` records
        const RamDomain* tuplePtr = recordTable.unpack(value, 2);
        const RamDomain branchId = tuplePtr[0];
        const RamDomain branchValue = tuplePtr[1];

        auto const branchInfo = sumInfo[branchId];
        auto const& branchName = branchInfo["name"].string_value();
        auto const& branchType = branchInfo["type"].string_value();

        destination << branchName << " ";

        switch (branchType[0]) {
            case 'i':
                destination << branchValue;
                break;
            case 'f':
                destination << ramBitCast<RamFloat>(branchValue);
                break;
            case 'u':
                destination << ramBitCast<RamUnsigned>(branchValue);
                break;
            case 's':
                destination << symbolTable.unsafeResolve(branchValue);
                break;
            case 'r':
                outputRecord(destination, branchValue, branchType);
                break;
            case '+':
                outputSum(destination, branchValue, branchType);
                break;
            default:
                assert(false && "Unsupported type attribute.");
        }
    }
};

class WriteStreamFactory {
public:
    virtual std::unique_ptr<WriteStream> getWriter(const IODirectives& ioDirectives,
            const SymbolTable& symbolTable, const RecordTable& recordTable) = 0;

    virtual const std::string& getName() const = 0;
    virtual ~WriteStreamFactory() = default;
};

template <>
inline void WriteStream::writeNext(const RamDomain* tuple) {
    writeNextTuple(tuple);
}

} /* namespace souffle */
