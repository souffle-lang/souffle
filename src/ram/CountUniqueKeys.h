/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file CountUniqueKeys.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/RelationStatement.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class CountUniqueKeys
 * @brief Count the number of unique keys on a relation given the key columns etc.
 *
 * This statement counts the number of unique keys in a relation.

* For example:
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
* UNIQUEKEYCOUNT rel A0 = 1, A1
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *
 * Counts the number of tuples in rel with a unique value for attribute 1,
 * while also having the first attribute with a value of 1
 */
class CountUniqueKeys : public RelationStatement {
public:
    CountUniqueKeys(std::string rel, const std::vector<std::size_t>& columns,
            const std::map<std::size_t, const ram::Expression*>& keyToConstants, bool isRecursive)
            : RelationStatement(rel), keyColumns(columns), recursiveRelation(isRecursive) {
        // copy the constants over
        for (auto [k, constant] : keyToConstants) {
            auto clonedConstant = clone(constant);
            constantsMap[k] = clonedConstant.get();
            constants.push_back(std::move(clonedConstant));
        }
    }

    const std::vector<std::size_t>& getKeyColumns() const {
        return keyColumns;
    }

    const std::map<std::size_t, const ram::Expression*>& getConstantsMap() const {
        return constantsMap;
    }

    bool isRecursiveRelation() const {
        return recursiveRelation;
    }

    CountUniqueKeys* cloning() const override {
        return new CountUniqueKeys(relation, keyColumns, constantsMap, recursiveRelation);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << (recursiveRelation ? "REC" : "") << "UNIQUEKEYCOUNT " << relation << " ";
        bool first = true;
        for (auto k : keyColumns) {
            if (first) {
                first = false;
            } else {
                os << ", ";
            }
            os << "A" << k;
            if (constantsMap.count(k)) {
                os << " = " << *constantsMap.at(k);
            }
        }
        os << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<CountUniqueKeys>(node);
        return RelationStatement::equal(other) && keyColumns == other.getKeyColumns() &&
               constantsMap == other.getConstantsMap() && recursiveRelation == other.isRecursiveRelation();
    }

    std::vector<std::size_t> keyColumns;
    std::map<std::size_t, const ram::Expression*> constantsMap;
    VecOwn<const ram::Expression> constants;
    bool recursiveRelation;
};

}  // namespace souffle::ram
