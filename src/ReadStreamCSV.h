/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReadStreamCSV.h
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "ReadStream.h"
#include "SymbolTable.h"
#include "utility/ContainerUtil.h"
#include "utility/FileUtil.h"
#include "utility/StringUtil.h"

#ifdef USE_LIBZ
#include "gzfstream.h"
#else
#include <fstream>
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace souffle {
class RecordTable;

class ReadStreamCSV : public ReadStream {
public:
    ReadStreamCSV(std::istream& file, const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable)
            : ReadStream(rwOperation, symbolTable, recordTable),
              delimiter(getOr(rwOperation, "delimiter", "\t")), file(file), lineNumber(0),
              inputMap(getInputColumnMap(rwOperation, arity)) {
        while (inputMap.size() < arity) {
            int size = static_cast<int>(inputMap.size());
            inputMap[size] = size;
        }
    }

protected:
    /**
     * Read and return the next tuple.
     *
     * Returns nullptr if no tuple was readable.
     * @return
     */
    std::unique_ptr<RamDomain[]> readNextTuple() override {
        if (file.eof()) {
            return nullptr;
        }
        std::string line;
        std::unique_ptr<RamDomain[]> tuple = std::make_unique<RamDomain[]>(typeAttributes.size());

        if (!getline(file, line)) {
            return nullptr;
        }
        // Handle Windows line endings on non-Windows systems
        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.length() - 1);
        }
        ++lineNumber;

        size_t start = 0;
        size_t end = 0;
        size_t columnsFilled = 0;
        for (uint32_t column = 0; columnsFilled < arity; column++) {
            size_t charactersRead = 0;
            std::string element = nextElement(line, start, end);
            if (inputMap.count(column) == 0) {
                continue;
            }
            ++columnsFilled;

            try {
                auto&& ty = typeAttributes.at(inputMap[column]);
                switch (ty[0]) {
                    case 's': {
                        tuple[inputMap[column]] = symbolTable.unsafeLookup(element);
                        charactersRead = element.size();
                        break;
                    }
                    case 'r': {
                        tuple[inputMap[column]] = readRecord(element, ty, 0, &charactersRead);
                        break;
                    }
                    case 'i': {
                        tuple[inputMap[column]] = RamSignedFromString(element, &charactersRead);
                        break;
                    }
                    case 'u': {
                        tuple[inputMap[column]] = ramBitCast(readRamUnsigned(element, charactersRead));
                        break;
                    }
                    case 'f': {
                        tuple[inputMap[column]] = ramBitCast(RamFloatFromString(element, &charactersRead));
                        break;
                    }
                    default: fatal("invalid type attribute: `%c`", ty[0]);
                }
                // Check if everything was read.
                if (charactersRead != element.size()) {
                    throw std::invalid_argument(
                            "Expected: " + delimiter + " or \\n. Got: " + element[charactersRead]);
                }
            } catch (...) {
                std::stringstream errorMessage;
                errorMessage << "Error converting <" + element + "> in column " << column + 1 << " in line "
                             << lineNumber << "; ";
                throw std::invalid_argument(errorMessage.str());
            }
        }

        return tuple;
    }

    /**
     * Read an unsigned element. Possible bases are 2, 10, 16
     * Base is indicated by the first two chars.
     */
    RamUnsigned readRamUnsigned(const std::string& element, size_t& charactersRead) {
        // Sanity check
        assert(element.size() > 0);

        RamSigned value = 0;

        // Check prefix and parse the input.
        if (isPrefix("0b", element)) {
            value = RamUnsignedFromString(element, &charactersRead, 2);
        } else if (isPrefix("0x", element)) {
            value = RamUnsignedFromString(element, &charactersRead, 16);
        } else {
            value = RamUnsignedFromString(element, &charactersRead);
        }
        return value;
    }

    std::string nextElement(const std::string& line, size_t& start, size_t& end) {
        std::string element;

        // Handle record/tuple delimiter coincidence.
        if (delimiter.find(',') != std::string::npos) {
            int record_parens = 0;
            size_t next_delimiter = line.find(delimiter, start);

            // Find first delimiter after the record.
            while (end < std::min(next_delimiter, line.length()) || record_parens != 0) {
                // Track the number of parenthesis.
                if (line[end] == '[') {
                    ++record_parens;
                } else if (line[end] == ']') {
                    --record_parens;
                }

                // Check for unbalanced parenthesis.
                if (record_parens < 0) {
                    break;
                };

                ++end;

                // Find a next delimiter if the old one is invalid.
                // But only if inside the unbalance parenthesis.
                if (end == next_delimiter && record_parens != 0) {
                    next_delimiter = line.find(delimiter, end);
                }
            }

            // Handle the end-of-the-line case where parenthesis are unbalanced.
            if (record_parens != 0) {
                std::stringstream errorMessage;
                errorMessage << "Unbalanced record parenthesis " << lineNumber << "; ";
                throw std::invalid_argument(errorMessage.str());
            }
        } else {
            end = std::min(line.find(delimiter, start), line.length());
        }

        // Check for missing value.
        if (start > end) {
            std::stringstream errorMessage;
            errorMessage << "Values missing in line " << lineNumber << "; ";
            throw std::invalid_argument(errorMessage.str());
        }

        element = line.substr(start, end - start);
        start = end + delimiter.size();

        return element;
    }

    std::map<int, int> getInputColumnMap(
            const std::map<std::string, std::string>& rwOperation, const unsigned arity_) const {
        std::string columnString = getOr(rwOperation, "columns", "");
        std::map<int, int> inputColumnMap;

        if (!columnString.empty()) {
            std::istringstream iss(columnString);
            std::string mapping;
            int index = 0;
            while (std::getline(iss, mapping, ':')) {
                inputColumnMap[stoi(mapping)] = index++;
            }
            if (inputColumnMap.size() < arity_) {
                throw std::invalid_argument("Invalid column set was given: <" + columnString + ">");
            }
        } else {
            while (inputColumnMap.size() < arity_) {
                int size = static_cast<int>(inputColumnMap.size());
                inputColumnMap[size] = size;
            }
        }
        return inputColumnMap;
    }

    const std::string delimiter;
    std::istream& file;
    size_t lineNumber;
    std::map<int, int> inputMap;
};

class ReadFileCSV : public ReadStreamCSV {
public:
    ReadFileCSV(const std::map<std::string, std::string>& rwOperation, SymbolTable& symbolTable,
            RecordTable& recordTable)
            : ReadStreamCSV(fileHandle, rwOperation, symbolTable, recordTable),
              baseName(souffle::baseName(getFileName(rwOperation))),
              fileHandle(getFileName(rwOperation), std::ios::in | std::ios::binary) {
        if (!fileHandle.is_open()) {
            throw std::invalid_argument("Cannot open fact file " + baseName + "\n");
        }
        // Strip headers if we're using them
        if (getOr(rwOperation, "headers", "false") == "true") {
            std::string line;
            getline(file, line);
        }
    }

    /**
     * Read and return the next tuple.
     *
     * Returns nullptr if no tuple was readable.
     * @return
     */
    std::unique_ptr<RamDomain[]> readNextTuple() override {
        try {
            return ReadStreamCSV::readNextTuple();
        } catch (std::exception& e) {
            std::stringstream errorMessage;
            errorMessage << e.what();
            errorMessage << "cannot parse fact file " << baseName << "!\n";
            throw std::invalid_argument(errorMessage.str());
        }
    }

    ~ReadFileCSV() override = default;

protected:
    /**
     * Return given filename or construct from relation name.
     * Default name is [configured path]/[relation name].facts
     *
     * @param rwOperation map of IO configuration options
     * @return input filename
     */
    static std::string getFileName(const std::map<std::string, std::string>& rwOperation) {
        auto name = getOr(rwOperation, "filename", rwOperation.at("name") + ".facts");
        if (name.front() != '/') {
            name = getOr(rwOperation, "fact-dir", ".") + "/" + name;
        }
        return name;
    }

    std::string baseName;
#ifdef USE_LIBZ
    gzfstream::igzfstream fileHandle;
#else
    std::ifstream fileHandle;
#endif
};

class ReadCinCSVFactory : public ReadStreamFactory {
public:
    std::unique_ptr<ReadStream> getReader(const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable) override {
        return std::make_unique<ReadStreamCSV>(std::cin, rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "stdin";
        return name;
    }
    ~ReadCinCSVFactory() override = default;
};

class ReadFileCSVFactory : public ReadStreamFactory {
public:
    std::unique_ptr<ReadStream> getReader(const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable) override {
        return std::make_unique<ReadFileCSV>(rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }

    ~ReadFileCSVFactory() override = default;
};

} /* namespace souffle */
