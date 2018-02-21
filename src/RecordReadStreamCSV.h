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
#include "RecordReadStream.h"
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

class RecordReadStreamCSV : public RecordReadStream {
public:
    RecordReadStreamCSV(std::istream& file, std::istream& symtab_file, SymbolTable& symbolTable,
            const IODirectives& ioDirectives)
            : RecordReadStream(symbolTable), delimiter(getDelimiter(ioDirectives)),
              file(file), symtab_file(symtab_file), lineNumber(0) {}

    ~RecordReadStreamCSV() override = default;

    /**
     * Read and return all the records
     *
     * Returns nullptr if no tuple was readable.
     * @return
     */
    std::unique_ptr<std::map<int, std::vector<RamDomain*>>> readAllRecords() override {
        if (file.eof()) {
            return nullptr;
        }
        std::string line;
        bool error = false;

	auto records = std::unique_ptr<std::map<int, std::vector<RamDomain*>>>(
	    new std::map<int, std::vector<RamDomain*>>());

	while(true) {
	    if (!getline(file, line)) {
		break;
	    }
	    // Handle Windows line endings on non-Windows systems
	    if (line.back() == '\r') {
		line = line.substr(0, line.length() - 1);
	    }
	    ++lineNumber;

	    size_t start = 0, end = 0;
	    /*RamDomain index = */std::stoll(getNextElement(line, start, end, 0, error));
	    size_t arity = std::stoul(getNextElement(line, start, end, 0, error));
	    RamDomain *record = new RamDomain[arity];

	    for (uint32_t column = 0; column < arity; ++column) {
		std::string elementStr = getNextElement(line, start, end, column, error);
		RamDomain element = std::stoll(elementStr);
		record[column] = element;
	    }

	    if (records->find(arity) == records->end()) {
		(*records)[arity] = std::vector<RamDomain*>();
	    }
	    (*records)[arity].push_back(record);


	    if (error) {
		throw std::invalid_argument("cannot parse records file");
	    }
	}

        return records;
    }

protected:
    std::string getNextElement(const std::string& line, size_t& start, size_t& end, uint32_t column, bool& error) {
	end = line.find(delimiter, start);
	if (end == std::string::npos) {
	    end = line.length();
	}
	std::string element;
	if (start <= end && end <= line.length()) {
	    element = line.substr(start, end - start);
	    if (element == "") {
		element = "n/a";
	    }
	} else {
	    if (!error) {
		std::stringstream errorMessage;
                    errorMessage << "Value missing in column " << column + 1 << " in line " << lineNumber
                                 << "; ";
                    throw std::invalid_argument(errorMessage.str());
	    }
	    element = "n/a";
	}
	start = end + delimiter.size();
	return element;
    }

    std::unique_ptr<std::string[]> readSymbolTable(int& numSymbols) override {
        if (symtab_file.eof()) {
            return nullptr;
        }
        std::string line;
        bool error = false;

        if (!getline(symtab_file, line)) {
            return nullptr;
        }
        // Handle Windows line endings on non-Windows systems
        if (line.back() == '\r') {
            line = line.substr(0, line.length() - 1);
        }

        size_t start = 0, end = 0;
	numSymbols = std::stoul(getNextElement(line, start, end, 0, error));
	std::unique_ptr<std::string[]> symtab(new std::string[numSymbols]);
	for (int i = 0; i < numSymbols; ++i) {
	    if (!getline(symtab_file, line))
		break;
	    start = 0;
	    end = 0;
	    std::string symbol = getNextElement(line, start, end, 0, error);
	    size_t index = std::stoul(getNextElement(line, start, end, 0, error));
	    symtab[index] = symbol;
	    if (error) {
		throw std::invalid_argument("cannot parse symbol table file");
	    }
	}
	return std::move(symtab);
    }

    std::string getDelimiter(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("delimiter")) {
            return ioDirectives.get("delimiter");
        }
        return "\t";
    }

    const std::string delimiter;
    std::istream& file;
    std::istream& symtab_file;
    size_t lineNumber;
};

class RecordReadFileCSV : public RecordReadStreamCSV {
public:
    RecordReadFileCSV(SymbolTable& symbolTable, const IODirectives& ioDirectives)
	: RecordReadStreamCSV(fileHandle, symtabFileHandle, symbolTable, ioDirectives),
	baseName(souffle::baseName(getFileName(ioDirectives))), fileHandle(getFileName(ioDirectives)),
	symtabFileHandle(getSymtabFileName(ioDirectives)) {
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
    std::unique_ptr<std::map<int, std::vector<RamDomain*>>> readAllRecords() override {
        try {
            return RecordReadStreamCSV::readAllRecords();
        } catch (std::exception& e) {
            std::stringstream errorMessage;
            errorMessage << e.what();
            errorMessage << "cannot parse fact file " << baseName << "!\n";
            throw std::invalid_argument(errorMessage.str());
        }
    }

    ~RecordReadFileCSV() override = default;

protected:
    std::string getFileName(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("filename")) {
            return ioDirectives.get("filename");
        }
        return ioDirectives.getRelationName() + ".facts";
    }
    std::string getSymtabFileName(const IODirectives& ioDirectives) const {
        if (ioDirectives.has("symtabfilename")) {
            return ioDirectives.get("symtabfilename");
        }
        return "souffle_symtab.facts";
    }
    std::string baseName;
#ifdef USE_LIBZ
    gzfstream::igzfstream fileHandle, symtabFileHandle;
#else
    std::ifstream fileHandle, symtabFileHandle;
#endif
};

/* class RecordReadCinCSVFactory : public RecordReadStreamFactory { */
/* public: */
/*     std::unique_ptr<RecordReadStream> getReader(const SymbolMask& symbolMask, SymbolTable& symbolTable, */
/*             const IODirectives& ioDirectives, const bool provenance) override { */
/*         return std::unique_ptr<RecordReadStreamCSV>( */
/*                 new RecordReadStreamCSV(std::cin, symbolMask, symbolTable, ioDirectives, provenance)); */
/*     } */
/*     const std::string& getName() const override { */
/*         static const std::string name = "stdin"; */
/*         return name; */
/*     } */
/*     ~ReadCinCSVFactory() override = default; */
/* }; */

class RecordReadFileCSVFactory : public RecordReadStreamFactory {
public:
    std::unique_ptr<RecordReadStream> getReader(SymbolTable& symbolTable,
            const IODirectives& ioDirectives) override {
        return std::unique_ptr<RecordReadFileCSV>(
                new RecordReadFileCSV(symbolTable, ioDirectives));
    }
    const std::string& getName() const override {
        static const std::string name = "file";
        return name;
    }

    ~RecordReadFileCSVFactory() override = default;
};

} /* namespace souffle */
