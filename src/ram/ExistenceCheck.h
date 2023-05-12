/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ExistenceCheck.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/AbstractExistenceCheck.h"
#include "ram/Expression.h"
#include "ram/Relation.h"
#include "souffle/utility/MiscUtil.h"
#include <memory>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class ExistenceCheck
 * @brief Existence check for a tuple(-pattern) in a relation
 *
 * Returns true if the tuple is in the relation
 *
 * The following condition is evaluated to true if the
 * tuple element t0.1 is in the relation A:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.1 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class ExistenceCheck : public AbstractExistenceCheck {
public:
    ExistenceCheck(std::string rel, VecOwn<Expression> vals) : AbstractExistenceCheck(NK_ExistenceCheck, rel, std::move(vals)) {}

    ExistenceCheck* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& cur : values) {
            newValues.emplace_back(cur->cloning());
        }
        return new ExistenceCheck(relation, std::move(newValues));
    }

    static bool classof(const Node* n){
        return n->getKind() == Node::NK_ExistenceCheck;
    }
};

}  // namespace souffle::ram
