/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Extend.h
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
 * @class Extend
 * @brief Extend equivalence relation.
 *
 * The following example merges A into B:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * EXTEND B WITH A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The source relation represents new knowledge, whereas the target relation is
 * old knowledge.
 */
class Extend : public BinRelationStatement {
public:
    Extend(std::string tRef, const std::string& sRef) : BinRelationStatement(sRef, tRef) {}

    /** @brief Get source relation */
    const std::string& getSourceRelation() const {
        return getFirstRelation();
    }

    /** @brief Get target relation */
    const std::string& getTargetRelation() const {
        return getSecondRelation();
    }

    Extend* cloning() const override {
        auto* res = new Extend(second, first);
        return res;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "EXTEND " << getTargetRelation() << " WITH " << getSourceRelation();
        os << std::endl;
    }
};

}  // namespace souffle::ram
