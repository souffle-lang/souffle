/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Swap.h
 *
 ***********************************************************************/

#pragma once

#include "ram/BinRelationStatement.h"
#include "ram/Relation.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class Swap
 * @brief Swap operation with respect to two relations
 *
 * Swaps the contents of the two relations
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * SWAP(A, B)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class Swap : public BinRelationStatement {
public:
    Swap(std::string f, std::string s) : BinRelationStatement(f, s) {}

    Swap* cloning() const override {
        return new Swap(first, second);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "SWAP (" << getFirstRelation() << ", " << getSecondRelation() << ")";
        os << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(SWAP " << getFirstRelation() << " " << getSecondRelation() << ")";
        os << std::endl;
    }
};

}  // namespace souffle::ram
