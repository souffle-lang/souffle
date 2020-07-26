/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReadStreamJSON.h
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "ReadStream.h"
#include "SymbolTable.h"
#include "utility/ContainerUtil.h"
#include "utility/FileUtil.h"
#include "utility/StringUtil.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace souffle {
class RecordTable;

class ReadStreamJSON : public ReadStream {
public:
    ReadStreamJSON(std::istream& file, const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable)
            : ReadStream(rwOperation, symbolTable, recordTable), file(file), pos(0), isInitialized(false) {
        std::string err;
        params = Json::parse(rwOperation.at("params"), err);
        if (err.length() > 0) {
            fatal("cannot get internal params: %s", err);
        }
    }

protected:
    std::istream& file;
    size_t pos;
    Json jsonSource;
    Json params;
    bool isInitialized;
    bool useObjects;
    std::map<const std::string, const size_t> paramIndex;

    std::unique_ptr<RamDomain[]> readNextTuple() override {
        // for some reasons we cannot initalized our json objects in constructor
        // otherwise it will segfault, so we initialize in the first call
        if (!isInitialized) {
            isInitialized = true;
            std::string error = "";
            std::string source(std::istreambuf_iterator<char>(file), {});

            jsonSource = Json::parse(source, error);
            // it should be wrapped by an extra array
            if (error.length() > 0 || !jsonSource.is_array()) {
                fatal("cannot deserialize json because %s:\n%s", error, source);
            }

            // we only check the first one, since there are extra checks
            // in readNextTupleObject/readNextTupleList
            if (jsonSource[0].is_array()) {
                useObjects = false;
            } else if (jsonSource[0].is_object()) {
                useObjects = true;
                size_t index_pos = 0;
                for (auto param : params["relation"]["params"].array_items()) {
                    paramIndex.insert(std::make_pair(param.string_value(), index_pos));
                    index_pos++;
                }
            } else {
                fatal("the input is neither list nor object format");
            }
        }

        if (useObjects) {
            return readNextTupleObject();
        } else {
            return readNextTupleList();
        }
    }

    std::unique_ptr<RamDomain[]> readNextTupleList() {
        if (pos >= jsonSource.array_items().size()) {
            return nullptr;
        }

        std::unique_ptr<RamDomain[]> tuple = std::make_unique<RamDomain[]>(typeAttributes.size());
        const Json& jsonObj = jsonSource[pos];
        assert(jsonObj.is_array() && "the input is not json array");
        pos++;
        for (size_t i = 0; i < typeAttributes.size(); ++i) {
            try {
                auto&& ty = typeAttributes.at(i);
                switch (ty[0]) {
                    case 's': {
                        tuple[i] = symbolTable.unsafeLookup(jsonObj[i].string_value());
                        break;
                    }
                    case 'r': {
                        tuple[i] = readNextElementList(jsonObj[i], ty);
                        break;
                    }
                    case 'i': {
                        tuple[i] = jsonObj[i].int_value();
                        break;
                    }
                    case 'u': {
                        tuple[i] = jsonObj[i].int_value();
                        break;
                    }
                    case 'f': {
                        tuple[i] = jsonObj[i].number_value();
                        break;
                    }
                    default: fatal("invalid type attribute: `%c`", ty[0]);
                }
            } catch (...) {
                std::stringstream errorMessage;
                if (jsonObj.is_array() && i < jsonObj.array_items().size()) {
                    errorMessage << "Error converting: " << jsonObj[i].dump();
                } else {
                    errorMessage << "Invalid index: " << i;
                }
                throw std::invalid_argument(errorMessage.str());
            }
        }

        return tuple;
    }

    RamDomain readNextElementList(const Json& source, const std::string& recordTypeName) {
        auto&& recordInfo = types["records"][recordTypeName];

        if (recordInfo.is_null()) {
            throw std::invalid_argument("Missing record type information: " + recordTypeName);
        }

        // Handle null case
        if (source.is_null()) {
            return 0;
        }

        assert(source.is_array() && "the input is not json array");
        auto&& recordTypes = recordInfo["types"];
        const size_t recordArity = recordInfo["arity"].long_value();
        std::vector<RamDomain> recordValues(recordArity);
        for (size_t i = 0; i < recordArity; ++i) {
            const std::string& recordType = recordTypes[i].string_value();
            switch (recordType[0]) {
                case 's': {
                    recordValues[i] = symbolTable.unsafeLookup(source[i].string_value());
                    break;
                }
                case 'r': {
                    recordValues[i] = readNextElementList(source[i], recordType);
                    break;
                }
                case 'i': {
                    recordValues[i] = source[i].int_value();
                    break;
                }
                case 'u': {
                    recordValues[i] = source[i].int_value();
                    break;
                }
                case 'f': {
                    recordValues[i] = source[i].number_value();
                    break;
                }
                default: fatal("invalid type attribute");
            }
        }

        return recordTable.pack(recordValues.data(), recordValues.size());
    }

    std::unique_ptr<RamDomain[]> readNextTupleObject() {
        if (pos >= jsonSource.array_items().size()) {
            return nullptr;
        }

        std::unique_ptr<RamDomain[]> tuple = std::make_unique<RamDomain[]>(typeAttributes.size());
        const Json& jsonObj = jsonSource[pos];
        assert(jsonObj.is_object() && "the input is not json object");
        pos++;
        for (auto p : jsonObj.object_items()) {
            try {
                // get the corresponding position by parameter name
                if (paramIndex.find(p.first) == paramIndex.end()) {
                    fatal("invalid parameter: %s", p.first);
                }
                size_t i = paramIndex.at(p.first);
                auto&& ty = typeAttributes.at(i);
                switch (ty[0]) {
                    case 's': {
                        tuple[i] = symbolTable.unsafeLookup(p.second.string_value());
                        break;
                    }
                    case 'r': {
                        tuple[i] = readNextElementObject(p.second, ty);
                        break;
                    }
                    case 'i': {
                        tuple[i] = p.second.int_value();
                        break;
                    }
                    case 'u': {
                        tuple[i] = p.second.int_value();
                        break;
                    }
                    case 'f': {
                        tuple[i] = p.second.number_value();
                        break;
                    }
                    default: fatal("invalid type attribute: `%c`", ty[0]);
                }
            } catch (...) {
                std::stringstream errorMessage;
                errorMessage << "Error converting: " << p.second.dump();
                throw std::invalid_argument(errorMessage.str());
            }
        }

        return tuple;
    }

    RamDomain readNextElementObject(const Json& source, const std::string& recordTypeName) {
        auto&& recordInfo = types["records"][recordTypeName];
        const std::string recordName = recordTypeName.substr(2);
        std::map<const std::string, const size_t> recordIndex;

        size_t index_pos = 0;
        for (auto param : params["records"][recordName]["params"].array_items()) {
            recordIndex.insert(std::make_pair(param.string_value(), index_pos));
            index_pos++;
        }

        if (recordInfo.is_null()) {
            throw std::invalid_argument("Missing record type information: " + recordTypeName);
        }

        // Handle null case
        if (source.is_null()) {
            return 0;
        }

        assert(source.is_object() && "the input is not json object");
        auto&& recordTypes = recordInfo["types"];
        const size_t recordArity = recordInfo["arity"].long_value();
        std::vector<RamDomain> recordValues(recordArity);
        recordValues.reserve(recordIndex.size());
        for (auto readParam : source.object_items()) {
            // get the corresponding position by parameter name
            if (recordIndex.find(readParam.first) == recordIndex.end()) {
                fatal("invalid parameter: %s", readParam.first);
            }
            size_t i = recordIndex.at(readParam.first);
            auto&& type = recordTypes[i].string_value();
            switch (type[0]) {
                case 's': {
                    recordValues[i] = symbolTable.unsafeLookup(readParam.second.string_value());
                    break;
                }
                case 'r': {
                    recordValues[i] = readNextElementObject(readParam.second, type);
                    break;
                }
                case 'i': {
                    recordValues[i] = readParam.second.int_value();
                    break;
                }
                case 'u': {
                    recordValues[i] = readParam.second.int_value();
                    break;
                }
                case 'f': {
                    recordValues[i] = readParam.second.number_value();
                    break;
                }
                default: fatal("invalid type attribute: `%c`", type[0]);
            }
        }

        return recordTable.pack(recordValues.data(), recordValues.size());
    }
};

class ReadFileJSON : public ReadStreamJSON {
public:
    ReadFileJSON(const std::map<std::string, std::string>& rwOperation, SymbolTable& symbolTable,
            RecordTable& recordTable)
            : ReadStreamJSON(fileHandle, rwOperation, symbolTable, recordTable),
              baseName(souffle::baseName(getFileName(rwOperation))),
              fileHandle(getFileName(rwOperation), std::ios::in | std::ios::binary) {
        if (!fileHandle.is_open()) {
            throw std::invalid_argument("Cannot open json file " + baseName + "\n");
        }
    }

    ~ReadFileJSON() override = default;

protected:
    /**
     * Return given filename or construct from relation name.
     * Default name is [configured path]/[relation name].json
     *
     * @param rwOperation map of IO configuration options
     * @return input filename
     */
    static std::string getFileName(const std::map<std::string, std::string>& rwOperation) {
        auto name = getOr(rwOperation, "filename", rwOperation.at("name") + ".json");
        if (name.front() != '/') {
            name = getOr(rwOperation, "fact-dir", ".") + "/" + name;
        }
        return name;
    }

    std::string baseName;
    std::ifstream fileHandle;
};

class ReadCinJSONFactory : public ReadStreamFactory {
public:
    std::unique_ptr<ReadStream> getReader(const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable) override {
        return std::make_unique<ReadStreamJSON>(std::cin, rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "json";
        return name;
    }
    ~ReadCinJSONFactory() override = default;
};

class ReadFileJSONFactory : public ReadStreamFactory {
public:
    std::unique_ptr<ReadStream> getReader(const std::map<std::string, std::string>& rwOperation,
            SymbolTable& symbolTable, RecordTable& recordTable) override {
        return std::make_unique<ReadFileJSON>(rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "jsonfile";
        return name;
    }

    ~ReadFileJSONFactory() override = default;
};
}  // namespace souffle
