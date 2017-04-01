/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ReadStreamSQLite.h
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "ReadStream.h"
#include "SymbolMask.h"
#include "SymbolTable.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

namespace souffle {

class ReadStreamSQLite : public ReadStream {
public:
    ReadStreamSQLite(const std::string& dbFilename, const std::string& relationName,
            const SymbolMask& symbolMask, SymbolTable& symbolTable)
            : dbFilename(dbFilename), relationName(relationName), symbolMask(symbolMask),
              symbolTable(symbolTable) {
        openDB();
        checkTableExists();
        prepareSelectStatement();
    }

    /**
     * Read and return the next tuple.
     *
     * Returns nullptr if no tuple was readable.
     * @return
     */
    std::unique_ptr<RamDomain[]> readNextTuple() override {
        if (sqlite3_step(selectStatement) != SQLITE_ROW) {
            return nullptr;
        }

        std::unique_ptr<RamDomain[]> tuple(new RamDomain[symbolMask.getArity()]);

        for (uint32_t column = 0; column < symbolMask.getArity(); column++) {
            std::string element(reinterpret_cast<const char*>(sqlite3_column_text(selectStatement, column)));

            if (element == "") {
                element = "n/a";
            }
            if (symbolMask.isSymbol(column)) {
                tuple[column] = symbolTable.lookup(element.c_str());
            } else {
                try {
                    tuple[column] = std::stoi(element.c_str());
                } catch (...) {
                    std::stringstream errorMessage;
                    errorMessage << "Error converting number in column " << (column) + 1;
                    throw std::invalid_argument(errorMessage.str());
                }
            }
        }

        return tuple;
    }

    ~ReadStreamSQLite() override {
        sqlite3_finalize(selectStatement);
        sqlite3_close(db);
    }

private:
    void executeSQL(const std::string& sql) {
        assert(db && "Database connection is closed");

        char* errorMessage = nullptr;
        /* Execute SQL statement */
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errorMessage);
        if (rc != SQLITE_OK) {
            std::stringstream error;
            error << "SQLite error in sqlite3_exec: " << sqlite3_errmsg(db) << "\n";
            error << "SQL error: " << errorMessage << "\n";
            error << "SQL: " << sql << "\n";
            sqlite3_free(errorMessage);
            throw std::invalid_argument(error.str());
        }
    }

    void throwError(std::string message) {
        std::stringstream error;
        error << message << sqlite3_errmsg(db) << "\n";
        throw std::invalid_argument(error.str());
    }

    void prepareSelectStatement() {
        std::stringstream selectSQL;
        selectSQL << "SELECT * FROM '" << relationName << "'";
        const char* tail = nullptr;
        if (sqlite3_prepare_v2(db, selectSQL.str().c_str(), -1, &selectStatement, &tail) != SQLITE_OK) {
            throwError("SQLite error in sqlite3_prepare_v2: ");
        }
    }

    void openDB() {
        if (sqlite3_open(dbFilename.c_str(), &db) != SQLITE_OK) {
            throwError("SQLite error in sqlite3_open: ");
        }
        sqlite3_extended_result_codes(db, 1);
        executeSQL("PRAGMA synchronous = OFF");
        executeSQL("PRAGMA journal_mode = MEMORY");
    }

    void checkTableExists() {
        sqlite3_stmt* tableStatement;
        std::stringstream selectSQL;
        selectSQL << "SELECT count(*) FROM sqlite_master WHERE type IN ('table', 'view') AND ";
        selectSQL << " name IN ('" << relationName << "', '_" << relationName << "');";
        const char* tail = nullptr;

        if (sqlite3_prepare_v2(db, selectSQL.str().c_str(), -1, &tableStatement, &tail) != SQLITE_OK) {
            throwError("SQLite error in sqlite3_prepare_v2: ");
        }

        if (sqlite3_step(tableStatement) == SQLITE_ROW) {
            int count = sqlite3_column_int(tableStatement, 0);
            if (count == 2) {
                sqlite3_finalize(tableStatement);
                return;
            }
        }
        sqlite3_finalize(tableStatement);
        throw std::invalid_argument("Required table and view does not exist for relation " + relationName);
    }
    const std::string& dbFilename;
    const std::string& relationName;
    const SymbolMask& symbolMask;
    SymbolTable& symbolTable;
    sqlite3_stmt* selectStatement;
    sqlite3* db;
};

class ReadStreamSQLiteFactory : public ReadStreamFactory {
public:
    std::unique_ptr<ReadStream> getReader(const SymbolMask& symbolMask, SymbolTable& symbolTable,
            const IODirectives& ioDirectives) override {
        std::string dbName = ioDirectives.get("dbname");
        std::string relationName = ioDirectives.getRelationName();
        return std::unique_ptr<ReadStreamSQLite>(
                new ReadStreamSQLite(dbName, relationName, symbolMask, symbolTable));
    }
    const std::string& getName() const override {
        static const std::string name = "sqlite";
        return name;
    }
    ~ReadStreamSQLiteFactory() override = default;
};

} /* namespace souffle */
