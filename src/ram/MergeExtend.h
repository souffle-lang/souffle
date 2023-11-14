/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file MergeExtend.h
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
 * @class MergeExtend
 * @brief MergeExtend equivalence relation.
 *
 * The following example merges A into B:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * MERGE EXTEND B WITH A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class MergeExtend : public BinRelationStatement {
public:
    MergeExtend(std::string tRef, const std::string& sRef) : BinRelationStatement(sRef, tRef) {}

    /** @brief Get source relation */
    const std::string& getSourceRelation() const {
        return getFirstRelation();
    }

    /** @brief Get target relation */
    const std::string& getTargetRelation() const {
        return getSecondRelation();
    }

    MergeExtend* cloning() const override {
        auto* res = new MergeExtend(second, first);
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "MERGE-EXTEND " << getTargetRelation() << " WITH " << getSourceRelation();
        os << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(MERGE_EXTEND " << getTargetRelation() << " " << getSourceRelation() << ")";
        os << std::endl;
    }
};

}  // namespace souffle::ram
