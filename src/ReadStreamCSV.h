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

#include "IODirectives.h"
#include "RamTypes.h"
#include "ReadStream.h"
#include "SymbolTable.h"
#include "Util.h"

#ifdef USE_LIBZ
#include "gzfstream.h"
#else
#include <fstream>
#endif

#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace souffle {

class ReadStreamCSV : public ReadStream {
public:
    ReadStreamCSV(std::istream& file, const std::vector<bool>& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance = false)
            : ReadStream(symbolMask, symbolTable, provenance), delimiter(getDelimiter(ioDirectives)),
              file(file), lineNumber(0), inputMap(getInputColumnMap(ioDirectives, arity)) {
        while (inputMap.size() < arity) {
            int size = inputMap.size();
            inputMap[size] = size;
        }
    }

    ~ReadStreamCSV() override = default;

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
        std::unique_ptr<RamDomain[]> tuple = std::make_unique<RamDomain[]>(symbolMask.size());

        if (!getline(file, line)) {
            return nullptr;
        }
        // Handle Windows line endings on non-Windows systems
        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.length() - 1);
        }
        ++lineNumber;

        size_t start = 0, end = 0, columnsFilled = 0;
        for (uint32_t column = 0; columnsFilled < arity; column++) {
            end = line.find(delimiter, start);
            if (end == std::string::npos) {
                end = line.length();
            }
            std::string element;
            if (start <= end) {
                element = line.substr(start, end - start);
            } else {
                std::stringstream errorMessage;
                errorMessage << "Values missing in line " << lineNumber << "; ";
                throw std::invalid_argument(errorMessage.str());
            }
            start = end + delimiter.size();
            if (inputMap.count(column) == 0) {
                continue;
            }
            ++columnsFilled;
            if (symbolMask.at(inputMap[column])) {
                tuple[inputMap[column]] = symbolTable.unsafeLookup(element);
            } else {
                try {
#if RAM_DOMAIN_SIZE == 64
                    tuple[inputMap[column]] = std::stoll(element);
#else
                    tuple[inputMap[column]] = std::stoi(element);
#endif
                } catch (...) {
                    std::stringstream errorMessage;
                    errorMessage << "Error converting number <" + element + "> in column " << column + 1
                                 << " in line " << lineNumber << "; ";
                    throw std::invalid_argument(errorMessage.str());
                }
            }
        }

        return tuple;
    }

    std::string getDelimiter(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("delimiter")) {
            return ioDirectives.get("delimiter");
        }
        return "\t";
    }

    std::map<int, int> getInputColumnMap(const IODirectives& ioDirectives, const unsigned arity) const {
        std::string columnString = "";
        if (ioDirectives.has("columns")) {
            columnString = ioDirectives.get("columns");
        }
        std::map<int, int> inputMap;

        if (!columnString.empty()) {
            std::istringstream iss(columnString);
            std::string mapping;
            int index = 0;
            while (std::getline(iss, mapping, ':')) {
                inputMap[stoi(mapping)] = index++;
            }
            if (inputMap.size() < arity) {
                throw std::invalid_argument("Invalid column set was given: <" + columnString + ">");
            }
        } else {
            while (inputMap.size() < arity) {
                int size = inputMap.size();
                inputMap[size] = size;
            }
        }
        return inputMap;
    }

    const std::string delimiter;
    std::istream& file;
    size_t lineNumber;
    std::map<int, int> inputMap;
};

class ReadFileCSV : public ReadStreamCSV {
public:
    ReadFileCSV(const std::vector<bool>& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance = false)
            : ReadStreamCSV(fileHandle, symbolMask, symbolTable, ioDirectives, provenance),
              baseName(souffle::baseName(getFileName(ioDirectives))),
              fileHandle(getFileName(ioDirectives), std::ios::in | std::ios::binary) {
        if (!ioDirectives.has("intermediate")) {
            if (!fileHandle.is_open()) {
                throw std::invalid_argument("Cannot open fact file " + baseName + "\n");
            }
            // Strip headers if we're using them
            if (ioDirectives.has("headers") && ioDirectives.get("headers") == "true") {
                std::string line;
                getline(file, line);
            }
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
    std::string getFileName(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("filename")) {
            return ioDirectives.get("filename");
        }
        return ioDirectives.getRelationName() + ".facts";
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
    std::unique_ptr<ReadStream> getReader(const std::vector<bool>& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) override {
        return std::make_unique<ReadStreamCSV>(std::cin, symbolMask, symbolTable, ioDirectives, provenance);
    }
    const std::string& getName() const override {
        static const std::string name = "stdin";
        return name;
    }
    ~ReadCinCSVFactory() override = default;
};

class ReadFileCSVFactory : public ReadStreamFactory {
public:
    std::unique_ptr<ReadStream> getReader(const std::vector<bool>& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives, const bool provenance) override {
        return std::make_unique<ReadFileCSV>(symbolMask, symbolTable, ioDirectives, provenance);
    }
    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }

    ~ReadFileCSVFactory() override = default;
};

} /* namespace souffle */
