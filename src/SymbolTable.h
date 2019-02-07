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
#include "RamTypes.h"
#include "Util.h"

#ifdef USE_MPI
#include "Mpi.h"
#include <thread>
#endif

#include <cassert>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <string>
#include <unordered_map>

namespace souffle {

/**
 * @class SymbolTable
 *
 * Global pool of re-usable strings
 *
 * SymbolTable stores Datalog symbols and converts them to numbers and vice versa.
 */
class SymbolTable {
#ifdef USE_MPI

private:
    enum {
        EXIT = 0,
        INSERT_STRING = 1,
        INSERT_VECTOR_STRING = 2,
        LOOKUP = 3,
        LOOKUP_EXISTING = 4,
        PRINT = 5,
        RESOLVE = 6,
        SIZE = 7,
        UNSAFE_LOOKUP = 8,
        UNSAFE_RESOLVE = 9
    };

    mutable std::unordered_map<std::string, size_t> strToNumCache;
    mutable std::unordered_map<size_t, std::string> numToStrCache;

    RamDomain cacheLookup(const std::string& symbol, const int tag) const {
        auto lease = access.acquire();
        (void)lease;  // avoid warning;
        auto it = strToNumCache.find(symbol);
        if (it != strToNumCache.end()) {
            return it->second;
        }
        mpi::send(symbol, 0, tag);
        RamDomain index;
        mpi::recv(index, 0, tag);
        strToNumCache.insert(std::pair<std::string, size_t>(symbol, index));
        return numToStrCache.insert(std::pair<size_t, std::string>(index, symbol)).first->first;
    }
    const std::string& cacheResolve(const RamDomain index, const int tag) const {
        auto lease = access.acquire();
        (void)lease;  // avoid warning;
        auto it = numToStrCache.find(index);
        if (it != numToStrCache.end()) {
            return it->second;
        }
        mpi::send(index, 0, tag);
        std::string symbol;
        mpi::recv(symbol, 0, tag);
        numToStrCache.insert(std::pair<size_t, std::string>(index, symbol));
        return strToNumCache.insert(std::pair<std::string, size_t>(symbol, index)).first->first;
    }

public:
    void handleMpiMessages(const size_t count) {
        assert(mpi::commRank() == 0);
        auto semaphor = count;
        while (semaphor) {
            auto status = mpi::probe();
            switch (status->MPI_TAG) {
                case EXIT: {
                    mpi::recv(status->MPI_SOURCE, EXIT);
                    --semaphor;
                    break;
                }
                case LOOKUP: {
                    std::string symbol;
                    mpi::recv(symbol, status);
                    mpi::send(lookup(symbol), status);
                    break;
                }
                case LOOKUP_EXISTING: {
                    std::string symbol;
                    mpi::recv(symbol, status);
                    mpi::send(lookupExisting(symbol), status);
                    break;
                }
                case UNSAFE_LOOKUP: {
                    std::string symbol;
                    mpi::recv(symbol, status);
                    mpi::send(unsafeLookup(symbol), status);
                    break;
                }
                case RESOLVE: {
                    RamDomain index;
                    mpi::recv(index, status);
                    mpi::send(resolve(index), status);
                    break;
                }
                case UNSAFE_RESOLVE: {
                    RamDomain index;
                    mpi::recv(index, status);
                    mpi::send(unsafeResolve(index), status);
                    break;
                }
                case SIZE: {
                    mpi::recv(status);
                    mpi::send(size(), status);
                    break;
                }
                case PRINT: {
                    mpi::recv(status);
                    print(std::cout);
                    break;
                }
                case INSERT_STRING: {
                    std::string symbol;
                    mpi::recv(symbol, status);
                    insert(symbol);
                    break;
                }
                case INSERT_VECTOR_STRING: {
                    std::vector<std::string> symbols;
                    mpi::recv(symbols, status);
                    insert(symbols);
                    break;
                }
                default: {
                    throw std::runtime_error("Invalid parameter in SymbolTable::handleMpiMessages.");
                    break;
                }
            }
        }
        for (size_t i = 0; i < count; ++i) {
            mpi::send(i + 1, EXIT);
        }
    }

    static int numberOfTags() {
        // ok, so this looks stupid, but it just gives the size of the enum at the top
        return 10;
    }

    static int exitTag() {
        return (int)EXIT;
    }

#endif

private:
    /** A lock to synchronize parallel accesses */
    mutable Lock access;

    /** Map indices to strings. */
    std::deque<std::string> numToStr;

    /** Map strings to indices. */
    std::unordered_map<std::string, size_t> strToNum;

    /** Convenience method to place a new symbol in the table, if it does not exist, and return the index of
     * it. */
    inline size_t newSymbolOfIndex(const std::string& symbol) {
        size_t index;
        auto it = strToNum.find(symbol);
        if (it == strToNum.end()) {
            index = numToStr.size();
            strToNum[symbol] = index;
            numToStr.push_back(symbol);
        } else {
            index = it->second;
        }
        return index;
    }

    /** Convenience method to place a new symbol in the table, if it does not exist. */
    inline void newSymbol(const std::string& symbol) {
        if (strToNum.find(symbol) == strToNum.end()) {
            strToNum[symbol] = numToStr.size();
            numToStr.push_back(symbol);
        }
    }

public:
    /** Empty constructor. */
    SymbolTable() = default;

    /** Copy constructor, performs a deep copy. */
    SymbolTable(const SymbolTable& other) : numToStr(other.numToStr), strToNum(other.strToNum) {}

    /** Copy constructor for r-value reference. */
    SymbolTable(SymbolTable&& other) noexcept {
        numToStr.swap(other.numToStr);
        strToNum.swap(other.strToNum);
    }

    SymbolTable(std::initializer_list<std::string> symbols) {
        strToNum.reserve(symbols.size());
        for (const auto& symbol : symbols) {
            newSymbol(symbol);
        }
    }

    /** Destructor, frees memory allocated for all strings. */
    virtual ~SymbolTable() = default;

    /** Assignment operator, performs a deep copy and frees memory allocated for all strings. */
    SymbolTable& operator=(const SymbolTable& other) {
        if (this == &other) {
            return *this;
        }
        numToStr = other.numToStr;
        strToNum = other.strToNum;
        return *this;
    }

    /** Assignment operator for r-value references. */
    SymbolTable& operator=(SymbolTable&& other) noexcept {
        numToStr.swap(other.numToStr);
        strToNum.swap(other.strToNum);
        return *this;
    }

    /** Find the index of a symbol in the table, inserting a new symbol if it does not exist there
     * already. */
    RamDomain lookup(const std::string& symbol) {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            return cacheLookup(symbol, LOOKUP);
        } else
#endif
        {
            auto lease = access.acquire();
            (void)lease;  // avoid warning;
            return static_cast<RamDomain>(newSymbolOfIndex(symbol));
        }
    }

    /** Finds the index of a symbol in the table, giving an error if it's not found */
    RamDomain lookupExisting(const std::string& symbol) const {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            return cacheLookup(symbol, LOOKUP_EXISTING);
        } else
#endif
        {
            auto lease = access.acquire();
            (void)lease;  // avoid warning;
            auto result = strToNum.find(symbol);
            if (result == strToNum.end()) {
                std::cerr << "Error string not found in call to SymbolTable::lookupExisting.\n";
                exit(1);
            }
            return static_cast<RamDomain>(result->second);
        }
    }

    /** Find the index of a symbol in the table, inserting a new symbol if it does not exist there
     * already. */
    RamDomain unsafeLookup(const std::string& symbol) {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            return cacheLookup(symbol, UNSAFE_LOOKUP);
        } else
#endif
            return newSymbolOfIndex(symbol);
    }

    /** Find a symbol in the table by its index, note that this gives an error if the index is out of
     * bounds.
     */
    const std::string& resolve(const RamDomain index) const {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            return cacheResolve(index, RESOLVE);
        } else
#endif
        {
            auto lease = access.acquire();
            (void)lease;  // avoid warning;
            auto pos = static_cast<size_t>(index);
            if (pos >= size()) {
                // TODO: use different error reporting here!!
                std::cerr << "Error index out of bounds in call to SymbolTable::resolve.\n";
                exit(1);
            }
            return numToStr[pos];
        }
    }

    const std::string& unsafeResolve(const RamDomain index) const {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            return cacheResolve(index, UNSAFE_RESOLVE);
        } else
#endif
            return numToStr[static_cast<size_t>(index)];
    }

    /* Return the size of the symbol table, being the number of symbols it currently holds. */
    size_t size() const {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            mpi::send(0, SIZE);
            size_t size;
            mpi::recv(size, 0, SIZE);
            return size;
        } else
#endif
            return numToStr.size();
    }

    /** Bulk insert symbols into the table, note that this operation is more efficient than repeated
     * inserts
     * of single symbols. */
    void insert(const std::vector<std::string>& symbols) {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            mpi::send(symbols, 0, INSERT_VECTOR_STRING);
        } else
#endif
        {
            auto lease = access.acquire();
            (void)lease;  // avoid warning;
            strToNum.reserve(size() + symbols.size());
            for (auto& symbol : symbols) {
                newSymbol(symbol);
            }
        }
    }

    /** Insert a single symbol into the table, not that this operation should not be used if inserting
     * symbols
     * in bulk. */
    void insert(const std::string& symbol) {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            mpi::send(symbol, 0, INSERT_STRING);
        } else
#endif
        {
            auto lease = access.acquire();
            (void)lease;  // avoid warning;
            newSymbol(symbol);
        }
    }

    /** Print the symbol table to the given stream. */
    void print(std::ostream& out) const {
#ifdef USE_MPI
        if (mpi::commRank() != 0) {
            mpi::send(0, PRINT);
        } else
#endif
        {
            out << "SymbolTable: {\n\t";
            out << join(strToNum, "\n\t",
                           [](std::ostream& out, const std::pair<std::string, std::size_t>& entry) {
                               out << entry.first << "\t => " << entry.second;
                           })
                << "\n";
            out << "}\n";
        }
    }

    Lock::Lease acquireLock() const {
        return access.acquire();
    }

    /** Stream operator, used as a convenience for print. */
    friend std::ostream& operator<<(std::ostream& out, const SymbolTable& table) {
        table.print(out);
        return out;
    }
};

}  // namespace souffle
