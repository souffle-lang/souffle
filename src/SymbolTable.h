/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file SymbolTable.h
 *
 * Data container to store symbols of the Datalog program. 
 *
 ***********************************************************************/

#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <iostream>
#include <fstream>
#include <string.h>

#include "Util.h"
#include "ParallelUtils.h"

namespace souffle {

/**
 * @class SymbolTable
 *
 * Global pool of re-usable strings
 *
 * SymbolTable stores Datalog symbols and converts them to numbers and vice versa.
 */
class SymbolTable {

    /** A lock to synchronize parallel accesses */
    mutable Lock access;

private:

    struct HashFunction {
            std::size_t operator() (const std::size_t &hash) const { return hash; }
    };

    struct HashEqual {
            bool operator() (const std::size_t & h1, const std::size_t & h2) const { return h1 == h2; }
    };

    std::unordered_map<size_t, const char*, HashFunction, HashEqual> symbolTable;

    static inline const size_t symbolTableHash(const char* str) {
        std::hash<std::string> hashFunction;
        return hashFunction(str);
    }

    /** Copy the referenced strings into the table. */
    inline void copyAll() {
        for (auto & symbol : symbolTable) symbol.second = strdup(symbol.second);
    }

    /** Free all the strings referenced in the table. */
    inline void freeAll() {
        for(auto symbol : symbolTable) free((void*) symbol.second);
    }

public:

    SymbolTable() { }

    SymbolTable(const SymbolTable& other)
        : symbolTable(other.symbolTable) {
        copyAll();
    }

    SymbolTable(SymbolTable&& other) {
        symbolTable.swap(other.symbolTable);
    }

    /** Destructor cleaning up strings */
    virtual ~SymbolTable() {
        freeAll();
    }

    /** Add support for an assignment operator */
    SymbolTable& operator=(const SymbolTable& other) {
        if (this == &other) return *this;
        freeAll();
        symbolTable = other.symbolTable;
        copyAll();
        return *this;
    }

    /** Add support for assignments from r-value references */
    SymbolTable& operator=(SymbolTable&& other) {
        symbolTable.swap(other.symbolTable);
        return *this;
    }

    /** Look-up a string given by a pointer to @p std::string in the pool and convert it to an index */
    size_t lookup(const char *str) {
        auto lease = access.acquire();
        (void) lease; // avoid warning;
        const size_t hash = symbolTableHash(str);
        if (symbolTable.find(hash) == symbolTable.end()) {
            const char* newstr = strdup(str);
            symbolTable[hash] = newstr;
        }
        return hash;
    }

    /** Lookup an index and convert it to a string */
    const char* resolve(const size_t hash) const {
        auto lease = access.acquire();
        (void) lease; // avoid warning;
        return symbolTable.find(hash)->second;
    }

    /* return size */
    size_t size() const {
        return symbolTable.size();
    }

    /** insert symbols from a constant string table */ 
    void insert(const char **symbols, const size_t n) {
        auto lease = access.acquire();
        (void) lease; // avoid warning;
        size_t hash;
        char* newstr;
        symbolTable.reserve(symbolTable.size() + n);
        for(size_t idx=0; idx < n; idx++) {
            const char* str = symbols[idx];
            hash = symbolTableHash(str);
            if (symbolTable.find(hash) == symbolTable.end()) {
                newstr = strdup(str);
                symbolTable[hash] = newstr;
            }
        }
    }

    /** inserts a single symbol into this table */
    void insert(const char* str) {
       auto lease = access.acquire();
       const size_t hash = symbolTableHash(str);
       if (symbolTable.find(hash) == symbolTable.end()) {
           const char* newstr = strdup(str);
           symbolTable[hash] = newstr;
       }
    }

    void print(std::ostream& out) const {
        out << "SymbolTable: {\n\t";
        out << join(symbolTable, "\n\t", [](std::ostream& out, const std::pair<std::size_t,const char*>& entry) {
            out << entry.second << "\t => " << entry.first;
        }) << "\n";
        out << "}\n";
    }

    friend std::ostream& operator<<(std::ostream& out, const SymbolTable& table) {
        table.print(out);
        return out;
    }

};

}
