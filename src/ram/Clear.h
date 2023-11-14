/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Clear.h
 *
 * Defines abstract class Statement and sub-classes for implementing the
 * Relational Algebra Machine (RAM), which is an abstract machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Relation.h"
#include "ram/RelationStatement.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ram {

/**
 * @class Clear
 * @brief Delete tuples of a relation
 *
 * This retains the target relation, but cleans its content
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * CLEAR A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

class Clear : public RelationStatement {
public:
    Clear(std::string rel) : RelationStatement(rel) {}

    Clear* cloning() const override {
        return new Clear(relation);
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "CLEAR " << relation << std::endl;
    }

    void print_sexpr(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "(CLEAR " << relation << ")" << std::endl;
    }
};

}  // namespace souffle::ram
