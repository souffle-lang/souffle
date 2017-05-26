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

#include "ParallelUtils.h"
#include "Util.h"

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

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
    /** Map indices to strings. */
    std::vector<char*> numToStr;

    /** Map strings to indices. */
    std::unordered_map<std::string, size_t> strToNum;

    /** Convenience method to copy strings between symbol tables and resolve their references. */
    inline void copyAll() {
        for (auto& cur : numToStr) {
            cur = strdup(cur);
        }
        for (auto& cur : strToNum) {
            const_cast<std::string&>(cur.first) = numToStr[cur.second];
        }
    }

    /** Convenience method to free memory allocated for strings. */
    inline void freeAll() {
        for (auto cur : numToStr) {
            free(cur);
        }
    }

    /** Convenience method to place a new symbol in the table, if it does not exist, and return the index of
     * it. */
    inline const size_t newSymbolOfIndex(const char* symbol) {
        size_t idx;
        auto it = strToNum.find(symbol);
        if (it == strToNum.end()) {
            char* str = strdup(symbol);
            idx = numToStr.size();
            strToNum[str] = idx;
            numToStr.push_back(str);
        } else {
            idx = it->second;
        }
        return idx;
    }

    /** Convenience method to place a new symbol in the table, if it does not exist. */
    inline void newSymbol(const char* symbol) {
        if (strToNum.find(symbol) == strToNum.end()) {
            char* str = strdup(symbol);
            strToNum[str] = numToStr.size();
            numToStr.push_back(str);
        }
    }

public:
    /** Empty constructor. */
    SymbolTable() {}

    /** Copy constructor, performs a deep copy. */
    SymbolTable(const SymbolTable& other) : numToStr(other.numToStr), strToNum(other.strToNum) {
        copyAll();
    }

    /** Copy constructor for r-value reference. */
    SymbolTable(SymbolTable&& other) noexcept {
        numToStr.swap(other.numToStr);
        strToNum.swap(other.strToNum);
    }

    /** Destructor, frees memory allocated for all strings. */
    virtual ~SymbolTable() {
        freeAll();
    }

    /** Assignment operator, performs a deep copy and frees memory allocated for all strings. */
    SymbolTable& operator=(const SymbolTable& other) {
        if (this == &other) {
            return *this;
        }
        freeAll();
        numToStr = other.numToStr;
        strToNum = other.strToNum;
        copyAll();
        return *this;
    }

    /** Assignment operator for r-value references. */
    SymbolTable& operator=(SymbolTable&& other) noexcept {
        numToStr.swap(other.numToStr);
        strToNum.swap(other.strToNum);
        return *this;
    }

    /** Find the index of a symbol in the table, inserting a new symbol if it does not exist there already. */
    const size_t lookup(const char* symbol) {
        auto lease = access.acquire();
        (void)lease;  // avoid warning;
        return newSymbolOfIndex(symbol);
    }

    /** Find a symbol in the table by its index, note that this gives an error if the index is out of bounds.
     */
    const char* resolve(const size_t idx) const {
        auto lease = access.acquire();
        (void)lease;  // avoid warning;
        if (idx >= size()) {
            // TODO: use different error reporting here!!
            std::cerr << "Error index out of bounds in call to SymbolTable::resolve.\n";
            exit(1);
        }
        return numToStr[idx];
    }

    const char* unsafeResolve(const size_t idx) const {
        return numToStr[idx];
    }

    /* Return the size of the symbol table, being the number of symbols it currently holds. */
    const size_t size() const {
        return numToStr.size();
    }

    /** Bulk insert symbols into the table, note that this operation is more efficient than repeated inserts
     * of single symbols. */
    void insert(const char** symbols, const size_t n) {
        auto lease = access.acquire();
        (void)lease;  // avoid warning;
        strToNum.reserve(size() + n);
        numToStr.reserve(size() + n);
        for (size_t idx = 0; idx < n; idx++) {
            newSymbol(symbols[idx]);
        }
    }

    /** Insert a single symbol into the table, not that this operation should not be used if inserting symbols
     * in bulk. */
    void insert(const char* symbol) {
        auto lease = access.acquire();
        (void)lease;  // avoid warning;
        newSymbol(symbol);
    }

    /** Print the symbol table to the given stream. */
    void print(std::ostream& out) const {
        out << "SymbolTable: {\n\t";
        out << join(strToNum, "\n\t", [](std::ostream& out,
                                              const std::pair<std::string, std::size_t>& entry) {
            out << entry.first << "\t => " << entry.second;
        }) << "\n";
        out << "}\n";
    }

    /** Stream operator, used as a convenience for print. */
    friend std::ostream& operator<<(std::ostream& out, const SymbolTable& table) {
        table.print(out);
        return out;
    }
};
}  // namespace souffle
