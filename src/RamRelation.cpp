/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamRelation.cpp
 *
 * Implements classes for indexed tables. Tuples are stored in blocks
 * chained with a simply linked list. Indexes follow the subscriber model,
 * i.e., an index is notified if a new tuple is inserted into the table.
 * Iterators are provided.
 *
 ***********************************************************************/

#include "RamRelation.h"
#include "RamIndex.h"
#include "SymbolMask.h"
#include "SymbolTable.h"

#include <iostream>
#include <list>

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace souffle {

/* print table in memory */
void RamRelation::store(std::vector<std::vector<std::string>>& result, const SymbolTable& symTable,
        const SymbolMask& mask) const {
    size_t cols = getArity();
    if (cols == 0 && !empty()) {
        std::vector<std::string> vec;
        vec.push_back("()");
        result.push_back(vec);
        return;
    }

    for (iterator it = begin(); it != end(); ++it) {
        std::vector<std::string> vec;
        const RamDomain* tuple = (*it);
        if (mask.isSymbol(0)) {
            std::string s = symTable.resolve(tuple[0]);
            vec.push_back(s);
        } else {
            vec.push_back(std::to_string(tuple[0]));
        }
        for (size_t col = 1; col < cols; col++) {
            if (mask.isSymbol(col)) {
                std::string s = symTable.resolve(tuple[col]);
                vec.push_back(s);
            } else {
                vec.push_back(std::to_string(tuple[col]));
            }
        }

        result.push_back(vec);
    }
}

/* input table from memory */
bool RamRelation::load(
        std::vector<std::vector<std::string>> data, SymbolTable& symTable, const SymbolMask& mask) {
    bool error = false;
    auto arity = getArity();
    for (std::vector<std::string> vec : data) {
        std::string line;
        RamDomain tuple[arity];
        uint32_t col = 0;
        for (std::string elem : vec) {
            if (mask.isSymbol(col)) {
                tuple[col] = symTable.lookup(elem.c_str());
            } else {
                try {
                    int32_t d;
                    if (elem.find('X') != std::string::npos || elem.find('x') != std::string::npos) {
                        d = std::stoll(elem.c_str(), nullptr, 16);
                    } else if (elem.find('b') != std::string::npos) {
                        d = std::stoll(elem.c_str(), nullptr, 2);
                    } else {
                        d = std::stoi(elem.c_str(), nullptr, 10);
                    }
                    tuple[col] = d;
                } catch (...) {
                    std::cerr << "Error converting to number\n";
                    error = true;
                }
            }
            col++;
        }
        if (!exists(tuple)) {
            insert(tuple);
        }
    }
    return error;
}

}  // end of namespace souffle
