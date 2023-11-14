/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file EstimateJoinSize.h
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
 * @class EstimateJoinSize
 * @brief Estimate the join size given the columns involved in the join (and constants)
 * The estimate is computed by dividing the relation size by the number of unique tuples projected on the join
columns
 *

* For example:
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ESTIMATEJOINSIZE rel A0 = 1, A1
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Estimates the size of the join on rel with a unique tuple value for attribute 1,
 * while also having the first attribute with a value of 1
 */

class EstimateJoinSize : public RelationStatement {
public:
    EstimateJoinSize(std::string rel, const std::set<std::size_t>& columns,
            const std::map<std::size_t, const ram::Expression*>& keyToConstants, bool isRecursive)
            : RelationStatement(rel), keyColumns(columns), recursiveRelation(isRecursive) {
        // copy the constants over
        for (auto [k, constant] : keyToConstants) {
            auto clonedConstant = clone(constant);
            constantsMap[k] = clonedConstant.get();
            constants.push_back(std::move(clonedConstant));
        }
    }

    const std::set<std::size_t>& getKeyColumns() const {
        return keyColumns;
    }

    const std::map<std::size_t, const ram::Expression*>& getConstantsMap() const {
        return constantsMap;
    }

    bool isRecursiveRelation() const {
        return recursiveRelation;
    }

    EstimateJoinSize* cloning() const override {
        return new EstimateJoinSize(relation, keyColumns, constantsMap, recursiveRelation);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << (recursiveRelation ? "REC" : "") << "ESTIMATEJOINSIZE " << relation
           << " ";
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

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "(ESTIMATEJOINSIZE " << (recursiveRelation ? "REC" : "NONREC") << " " << relation
           << " ";
        bool first = true;
        for (auto k : keyColumns) {
            if (first) {
                first = false;
            } else {
                os << "( ";
            }
            os << "A" << k;
            if (constantsMap.count(k)) {
                os << " " << *constantsMap.at(k);
            }
        }
        os << "))" << std::endl;
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<EstimateJoinSize>(node);
        return RelationStatement::equal(other) && keyColumns == other.getKeyColumns() &&
               constantsMap == other.getConstantsMap() && recursiveRelation == other.isRecursiveRelation();
    }

    std::set<std::size_t> keyColumns;
    std::map<std::size_t, const ram::Expression*> constantsMap;
    VecOwn<const ram::Expression> constants;
    bool recursiveRelation;
};

}  // namespace souffle::ram
