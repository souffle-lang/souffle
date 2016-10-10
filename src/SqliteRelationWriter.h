/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All Rights reserved
 * 
 * The Universal Permissive License (UPL), Version 1.0
 * 
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the 
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified 
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 * 
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 * 
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and 
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the 
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 * 
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/************************************************************************
 *
 * @file SqliteRelationWriter.h
 *
 * Defines classes/functions for outputting relation to an sqlite database.
 *
 ***********************************************************************/

#pragma once

#include <sqlite3.h>
#include <regex>
#include "SouffleInterface.h"

/* Execute SQL statement */
inline void executeSQL(std::string sql, sqlite3 *db) {
    assert(db && "Database connection is closed");

    char *zErrMsg = 0;
    /* Execute SQL statement */
    int rc = sqlite3_exec(db, sql.c_str(), NULL, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite error in sqlite3_exec: " << sqlite3_errmsg(db) << "\n";
        std::cerr << "SQL error: " << zErrMsg << "\n";
        std::cerr << "SQL: " << sql << "\n";
        sqlite3_free(zErrMsg);
        exit(1);
    }
}

class SqliteInserter {
private:
    sqlite3 *db;
    std::string relationName;
    unsigned int arity;
    sqlite3_stmt *insertStmt;
public:
    SqliteInserter(sqlite3 *db, std::string relationName, unsigned int arity) :
        db(db), relationName(relationName), arity(arity) {
        std::stringstream insertSQL;
        insertSQL << "INSERT INTO " << relationName << " VALUES ";
        insertSQL << "(";
        insertSQL << "@V0";
        for (unsigned int i = 1; i < arity; i++) {
            insertSQL << ",@V" << i;
        }
        insertSQL << ");";
        const char *tail = 0;
        if (sqlite3_prepare_v2(db, insertSQL.str().c_str(), -1, &insertStmt, &tail) != SQLITE_OK) {
            std::cerr << "SQLite error in sqlite3_prepare_v2: " << sqlite3_errmsg(db) << "\n";
            exit(1);
        }
    }

    void insert(const std::vector<std::string> &tuple) {
        assert(tuple.size() == arity);
        for (unsigned int i = 0; i < arity; i++) {
            if (sqlite3_bind_text(insertStmt, i+1, tuple[i].c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                std::cerr << "SQLite error in sqlite3_bind_text: " << sqlite3_errmsg(db) << "\n";
                exit(1);
            }
        }
        if (sqlite3_step(insertStmt) != SQLITE_DONE) {
            std::cerr << "SQLite error in sqlite3_step: " << sqlite3_errmsg(db) << "\n";
            exit(1);
        }
        sqlite3_clear_bindings(insertStmt);
        sqlite3_reset(insertStmt);
    }

};

class SqliteRelationWriter {
private:
    sqlite3 *db;
    std::string symbolTableName;
public:
    SqliteRelationWriter(sqlite3 *db, std::string symbolTableName) :
        db(db), symbolTableName(symbolTableName) {}

    void writeRelation(souffle::Relation *relation) {
        // Create table
        std::stringstream createStmt;
        std::string relationName = relation->getName();
        createStmt << "CREATE TABLE '_" << relationName << "'(";
        createStmt << "'" << relation->getAttrName(0) << "' ";
        createStmt << "INTEGER";
        for (unsigned int i = 1; i < relation->getArity(); i++) {
            createStmt << ",'" << relation->getAttrName(i) << "' ";
            createStmt << "INTEGER";
        }
        createStmt << ");";
        executeSQL(createStmt.str(), db);

        // Insert tuples
        executeSQL("BEGIN TRANSACTION", db);
        SqliteInserter relationInserter(db, "_" + relationName, relation->getArity());
        for (souffle::tuple output : *relation) {
            std::vector<std::string> tuple;
            for (unsigned int i = 0; i < output.size(); i++) {
                tuple.push_back(std::to_string(output[i]));
            }
            relationInserter.insert(tuple);
        }
        executeSQL("END TRANSACTION", db);

        // Create view with symbol strings resolved
        std::stringstream createViewStmt;
        createViewStmt << "CREATE VIEW '" << relationName << "' AS ";
        std::stringstream projectionClause;
        std::stringstream fromClause;
        fromClause << "'_" << relationName << "'";
        std::stringstream whereClause;
        bool firstWhere = true;
        for (unsigned int i = 0; i < relation->getArity(); i++) {
            std::string columnName = relation->getAttrName(i);
            if (i != 0) {
                projectionClause << ",";
            }
            if (relation->getAttrType(i)[0] == 'i' || relation->getAttrType(i)[0] == 'r') {
                projectionClause << "'_" << relationName << "'.'" << columnName << "'";
            } else if (relation->getAttrType(i)[0] == 's') {
                projectionClause << "'_symtab_" << columnName << "'.symbol AS '" << columnName << "'";
                fromClause << ",'" << symbolTableName << "' AS '_symtab_" << columnName << "'";
                if (!firstWhere) {
                    whereClause << " AND ";
                } else {
                    firstWhere = false;
                }
                whereClause << "'_" << relationName << "'.'" << columnName << "' = " << "'_symtab_" << columnName << "'.id";
            } else {
                assert(0 && "unknown type");
            }
        }
        createViewStmt << "SELECT " << projectionClause.str() << " FROM " << fromClause.str();
        if (!firstWhere) {
            createViewStmt << " WHERE " << whereClause.str();
        }
        createViewStmt << ";";
        executeSQL(createViewStmt.str(), db);
    }
};

/**
 * Outputs relations to a new sqlite database. The database schema consists of:
 *  - a symbol table, "__SymbolTable" (mapping symbol ids to their corresponding string)
 *  - a "_<relation-name>" table for each relation (in which symbols are referred to by their ids)
 *  - a "<relation-name>" view for each relation (in which symbols are resolved to their corresponding string)
 *
 * @param dbFilename database filename (if the file already exists then it will be overwritten)
 * @param prog program to output relations for
 * @param onlyOutput whether to only include output relations or to include all relations
 */
inline void writeRelationsToSqlite(std::string dbFilename, souffle::SouffleProgram *prog, bool onlyOutput) {
    sqlite3 *db;
    std::remove(dbFilename.c_str());
    if (sqlite3_open(dbFilename.c_str(), &db) != SQLITE_OK) {
        std::cerr << "SQLite error in sqlite3_open: " << sqlite3_errmsg(db) << "\n";
        exit(1);
    }
    sqlite3_extended_result_codes(db, 1);
    executeSQL("PRAGMA synchronous = OFF", db);
    executeSQL("PRAGMA journal_mode = MEMORY", db);

    const souffle::SymbolTable &symTable = prog->getSymbolTable();
    const std::string symbolTableName = "__SymbolTable";
    executeSQL("CREATE TABLE " + symbolTableName + " (id INTEGER PRIMARY KEY, symbol TEXT);", db);
    executeSQL("BEGIN TRANSACTION", db);
    SqliteInserter symTableInserter(db, symbolTableName, 2);
    for (unsigned int i = 0; i < symTable.size(); i++) {
        std::string symbol = symTable.resolve(i);
        symTableInserter.insert({std::to_string(i), symbol});
    }
    executeSQL("END TRANSACTION", db);

    SqliteRelationWriter relationWriter(db, symbolTableName);
    std::vector<souffle::Relation *> relations = onlyOutput ? prog->getOutputRelations() : prog->getAllRelations();
    for (souffle::Relation *relation : relations) {
        relationWriter.writeRelation(relation);
    }
    sqlite3_close(db);;
}
