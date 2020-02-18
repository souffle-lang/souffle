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
#include "RecordTable.h"
#include "SymbolTable.h"
#include "Util.h"
#include "json11.h"
#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace souffle {

using json11::Json;

class ReadStream {
protected:
    ReadStream(const IODirectives& ioDirectives, SymbolTable& symbolTable, RecordTable& recordTable)
            : symbolTable(symbolTable), recordTable(recordTable) {
        const std::string& relationName{ioDirectives.getRelationName()};

        std::string parseErrors;

        types = Json::parse(ioDirectives.get("types"), parseErrors);

        assert(parseErrors.size() == 0 && "Internal JSON parsing failed.");

        arity = static_cast<size_t>(types[relationName]["arity"].long_value());
        auxiliaryArity = static_cast<size_t>(types[relationName]["auxArity"].long_value());

        for (size_t i = 0; i < arity; ++i) {
            std::string type = types[relationName]["types"][i].string_value();
            typeAttributes.push_back(std::move(type));
        }
    }

public:
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
    /**
     * Read a record from a string.
     *
     * @param source - string containing a record
     * @param recordTypeName - record type.
     * @parem pos - start parsing from this position.
     * @param consumed - if not nullptr: number of characters read.
     *
     */
    RamDomain readRecord(const std::string& source, const std::string& recordTypeName, size_t pos = 0,
            size_t* _consumed = nullptr) {
        const size_t initial_position = pos;

        Json recordInfo = types["records"][recordTypeName];

        // Check if record type information are present
        if (recordInfo.is_null()) {
            throw std::invalid_argument("Missing record type information: " + recordTypeName);
        }

        // Handle nil case
        consumeWhiteSpace(source, pos);
        if (source.substr(pos, 3) == "nil") {
            if (_consumed != nullptr) {
                *_consumed = 3;
            }
            return recordTable.getNil();
        }

        const Json recordTypes = recordInfo["types"];
        const size_t recordArity = recordInfo["arity"].long_value();

        std::vector<RamDomain> recordValues(recordArity);

        consumeChar(source, '[', pos);

        for (size_t i = 0; i < recordArity; ++i) {
            const std::string& recordType = recordTypes[i].string_value();
            size_t consumed = 0;

            if (i > 0) {
                consumeChar(source, ',', pos);
            }
            consumeWhiteSpace(source, pos);
            switch (recordType[0]) {
                case 's':
                    recordValues[i] = readStringInRecord(source, pos, &consumed);
                    break;
                case 'i':
                    recordValues[i] = RamDomainFromString(source.substr(pos), &consumed);
                    break;
                case 'u':
                    recordValues[i] = ramBitCast(RamUnsignedFromString(source.substr(pos), &consumed));
                    break;
                case 'f':
                    recordValues[i] = ramBitCast(RamFloatFromString(source.substr(pos), &consumed));
                    break;
                case 'r':
                    recordValues[i] = readRecord(source, recordType, pos, &consumed);
                    break;
                case '+':
                    recordValues[i] = readSum(source, recordType, pos, &consumed);
                    break;
                default:
                    assert(false && "Invalid type attribute");
            }
            pos += consumed;
        }
        consumeChar(source, ']', pos);

        if (_consumed != nullptr) {
            *_consumed = pos - initial_position;
        }

        return recordTable.pack(recordValues);
    }

    /**
     * Read a sum type from a string.
     *
     * @param source - string containing a sum
     * @param sumTypeName - sum type.
     * @parem pos - start parsing from this position.
     * @param consumed - if not nullptr: number of characters read.
     *
     */
    RamDomain readSum(const std::string& source, const std::string& sumTypeName, size_t pos = 0,
            size_t* _consumed = nullptr) {
        const size_t initial_position = pos;

        auto const branchName = consumeLiteral(source, pos);
        consumeWhiteSpace(source, pos);

        auto const branches = types["sums"][sumTypeName];
        // Check if record type information are present
        if (!branches.is_array()) {
            throw std::invalid_argument("Missing sum type information: `" + sumTypeName + "`");
        }

        std::string const* branchType = nullptr;
        RamDomain branchIndex = 0;
        for (auto&& br : branches.array_items()) {
            if (br["name"].string_value() != branchName) {
                branchType = &br["type"].string_value();
                break;
            }

            branchIndex++;
        }

        if (!branchType) {
            throw std::invalid_argument(
                    "Unknown sum branch: `" + sumTypeName + "` `" + branchName.data() + "`");
        }

        size_t consumed = 0;
        RamDomain branchValue;
        switch ((*branchType)[0]) {
            case 's':
                branchValue = readStringInRecord(source, pos, &consumed);
                break;
            case 'i':
                branchValue = RamDomainFromString(source.substr(pos), &consumed);
                break;
            case 'u':
                branchValue = ramBitCast(RamUnsignedFromString(source.substr(pos), &consumed));
                break;
            case 'f':
                branchValue = ramBitCast(RamFloatFromString(source.substr(pos), &consumed));
                break;
            case 'r':
                branchValue = readRecord(source, *branchType, pos, &consumed);
                break;
            case '+':
                branchValue = readSum(source, *branchType, pos, &consumed);
                break;
            default:
                assert(false && "Invalid type attribute");
                exit(EXIT_FAILURE);
        }
        pos += consumed;

        if (_consumed != nullptr) {
            *_consumed = pos - initial_position;
        }

        return recordTable.pack({RamDomain(branchIndex), branchValue});
    }

    RamDomain readStringInRecord(const std::string& source, const size_t pos, size_t* _consumed) {
        size_t endOfSymbol = source.find_first_of(",]", pos);

        if (endOfSymbol == std::string::npos) {
            throw std::invalid_argument("Unexpected end of input in record");
        }

        *_consumed = endOfSymbol - pos;
        std::string str = source.substr(pos, *_consumed);

        return symbolTable.unsafeLookup(str);
    }

    /**
     * Read past given character, consuming any preceding whitespace.
     */
    void consumeChar(const std::string& str, char c, size_t& pos) {
        consumeWhiteSpace(str, pos);
        if (pos >= str.length()) {
            throw std::invalid_argument("Unexpected end of input in record");
        }
        if (str[pos] != c) {
            std::stringstream error;
            error << "Expected: \'" << c << "\', got: " << str[pos];
            throw std::invalid_argument(error.str());
        }
        ++pos;
    }

    /**
     * Read the next 'literal' (sequence of non-whitespace), consuming any preceding whitespace.
     */
    std::string_view consumeLiteral(const std::string& str, size_t& pos) {
        consumeWhiteSpace(str, pos);
        if (pos >= str.length()) {
            throw std::invalid_argument("Unexpected end of input in record");
        }

        const size_t bgn = pos;
        for (; pos < str.length() && !std::isspace(static_cast<unsigned char>(str[pos])); ++pos)
            ;

        return std::string_view(str.data() + bgn, pos - bgn);
    }

    /**
     * Advance position in the string until first non-whitespace character.
     */
    void consumeWhiteSpace(const std::string& str, size_t& pos) {
        while (pos < str.length() && std::isspace(static_cast<unsigned char>(str[pos]))) {
            ++pos;
        }
    }

    Json types;

    virtual std::unique_ptr<RamDomain[]> readNextTuple() = 0;
    std::vector<std::string> typeAttributes;
    SymbolTable& symbolTable;
    RecordTable& recordTable;

    size_t arity;
    size_t auxiliaryArity;
};

class ReadStreamFactory {
public:
    virtual std::unique_ptr<ReadStream> getReader(const IODirectives&, SymbolTable&, RecordTable&) = 0;
    virtual const std::string& getName() const = 0;
    virtual ~ReadStreamFactory() = default;
};

} /* namespace souffle */
