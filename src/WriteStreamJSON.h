/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file WriteStreamJSON.h
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "SymbolTable.h"
#include "WriteStream.h"
#include "json11.h"
#include "utility/ContainerUtil.h"

#include <map>
#include <ostream>
#include <queue>
#include <stack>
#include <string>
#include <variant>
#include <vector>

namespace souffle {

class WriteStreamJSON : public WriteStream {
protected:
    WriteStreamJSON(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStream(rwOperation, symbolTable, recordTable),
              beautify(getOr(rwOperation, "beautify", "false") == "true"){};

    const bool beautify;

    void writeNextTupleJSON(std::ostream& destination, const RamDomain* tuple) {
        std::vector<Json> result;

        destination << "[";
        for (size_t col = 0; col < arity; ++col) {
            if (col > 0) {
                destination << ", ";
            }
            writeNextTupleElement(destination, typeAttributes.at(col), tuple[col]);
        }

        // Output a JSON array for all tuples
        destination << "]";
    }

    void writeNextTupleElement(std::ostream& destination, const std::string& name, const RamDomain value) {
        using ValueTuple = std::pair<const std::string, const RamDomain>;
        std::stack<std::variant<ValueTuple, std::string>> worklist;
        worklist.push(std::make_pair(name, value));

        // the Json11 output is not tail recursive, therefore highly inefficient for recursive record
        // in addition the JSON object is immutable, so has memory overhead
        while (!worklist.empty()) {
            std::variant<ValueTuple, std::string> curr = worklist.top();
            worklist.pop();

            if (std::holds_alternative<std::string>(curr)) {
                destination << std::get<std::string>(curr);
                continue;
            }

            const std::string& currType = std::get<ValueTuple>(curr).first;
            const RamDomain currValue = std::get<ValueTuple>(curr).second;
            assert(currType.length() > 2 && "Invalid type length");
            switch (currType[0]) {
                // since some strings may need to be escaped, we use dump here
                case 's': destination << Json(symbolTable.unsafeResolve(currValue)).dump(); break;
                case 'i': destination << currValue; break;
                case 'u': destination << (int)ramBitCast<RamUnsigned>(currValue); break;
                case 'f': destination << ramBitCast<RamFloat>(currValue); break;
                case 'r': {
                    auto&& recordInfo = types["records"][currType];
                    assert(!recordInfo.is_null() && "Missing record type information");
                    if (currValue == 0) {
                        destination << "null";
                        break;
                    }

                    auto&& recordTypes = recordInfo["types"];
                    const size_t recordArity = recordInfo["arity"].long_value();
                    const RamDomain* tuplePtr = recordTable.unpack(currValue, recordArity);
                    worklist.push("]");
                    for (auto i = (long long)(recordArity - 1); i >= 0; --i) {
                        if (i != (long long)(recordArity - 1)) {
                            worklist.push(", ");
                        }
                        const std::string& recordType = recordTypes[i].string_value();
                        const RamDomain recordValue = tuplePtr[i];
                        worklist.push(std::make_pair(recordType, recordValue));
                    }

                    worklist.push("[");
                    break;
                }
                default: fatal("unsupported type attribute: `%c`", currType[0]);
            }
        }
    }
};

class WriteFileJSON : public WriteStreamJSON {
public:
    WriteFileJSON(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStreamJSON(rwOperation, symbolTable, recordTable), isFirst(true),
              file(rwOperation.at("filename"), std::ios::out | std::ios::binary) {
        file << "[";
    }

    ~WriteFileJSON() override {
        file << "]\n";
        file.close();
    }

protected:
    bool isFirst;
    std::ofstream file;

    void writeNullary() override {
        file << "null\n";
    }

    void writeNextTuple(const RamDomain* tuple) override {
        if (!isFirst) {
            file << ",\n";
        } else {
            isFirst = false;
        }
        writeNextTupleJSON(file, tuple);
    }
};

class WriteCoutJSON : public WriteStreamJSON {
public:
    WriteCoutJSON(const std::map<std::string, std::string>& rwOperation, const SymbolTable& symbolTable,
            const RecordTable& recordTable)
            : WriteStreamJSON(rwOperation, symbolTable, recordTable), isFirst(true) {
        std::cout << "[";
    }

    ~WriteCoutJSON() override {
        std::cout << "]\n";
    };

protected:
    bool isFirst;

    void writeNullary() override {
        std::cout << "null\n";
    }

    void writeNextTuple(const RamDomain* tuple) override {
        if (!isFirst) {
            std::cout << ",\n";
        } else {
            isFirst = false;
        }
        writeNextTupleJSON(std::cout, tuple);
    }
};

class WriteFileJSONFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) override {
        return std::make_unique<WriteFileJSON>(rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "jsonfile";
        return name;
    }

    ~WriteFileJSONFactory() override = default;
};

class WriteCoutJSONFactory : public WriteStreamFactory {
public:
    std::unique_ptr<WriteStream> getWriter(const std::map<std::string, std::string>& rwOperation,
            const SymbolTable& symbolTable, const RecordTable& recordTable) override {
        return std::make_unique<WriteCoutJSON>(rwOperation, symbolTable, recordTable);
    }

    const std::string& getName() const override {
        static const std::string name = "json";
        return name;
    }

    ~WriteCoutJSONFactory() override = default;
};
}  // namespace souffle