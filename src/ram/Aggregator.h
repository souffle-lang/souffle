/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2022, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Aggregator.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "souffle/utility/MiscUtil.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class Aggregator
 * @brief An aggregator defines the operation to apply on an aggregate.
 * It can be some built-in function or rely on a user-defined functor.
 */
class Aggregator {
public:
    virtual ~Aggregator() = default;

    bool operator==(const Aggregator& other) const {
        return this == &other || (typeid(*this) == typeid(other) && equal(other));
    }

    Own<Aggregator> cloneImpl() const {
        return Own<Aggregator>(cloning());
    }

    virtual bool equal(const Aggregator&) const {
        return true;
    }

    virtual std::vector<const Node*> getChildren() const {
        return {};
    }

    /**
     * @brief Create a cloning (i.e. deep copy) of this node
     */
    virtual Aggregator* cloning() const = 0;

    virtual void print(std::ostream& os, int tabpos) const = 0;

    virtual void print_sexpr(std::ostream& os, int tabpos) const = 0;
};

}  // namespace souffle::ram